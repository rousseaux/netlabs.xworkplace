
/*
 *@@sourcefile cnrsort.c:
 *      this file contains the new folder sort comparison functions only.
 *      It is used by the new sort methods in xfldr.c.
 *
 *      For introductory information on how XFolder extended sorting
 *      is implemented, refer to the XFolder programming guide.
 *
 *      This file was called "xfsort.c" in V0.80.
 *
 *      When sorting a container, one has to provide comparison
 *      functions which will be called by PM's internal sort
 *      algorithm (I don't know if it's a quicksort or whatever,
 *      but this doesn't matter anyway). In other words, one does
 *      _not_ have to write some sort algorithm oneself,
 *      but only provide a comparision function.
 *
 *      These functions are used in two contexts:
 *      1)  with the CM_SORTRECORD msg for sorting just once;
 *      2)  in the CNRINFO.pSortRecord field, when a container
 *          should always be sorted.
 *
 *      Sort comparison functions need to be like this:
 +          SHORT EXPENTRY fnCompareExt(PMINIRECORDCORE pmrc1,
 +                                      PMINIRECORDCORE pmrc2,
 +                                      PVOID pStorage)
 *
 *      Note: the information in the PM reference is flat out wrong.
 *      Container sort functions need to return the following:
 +          0   pmrc1 == pmrc2
 +         -1   pmrc1 <  pmrc2
 +         +1   pmrc1 >  pmrc2
 *
 *      All these sort functions are used by XFolder in the context
 *      of the extended sort functions. All the "pmrc" parameters
 *      therefore point to WPS MINIRECORDCOREs (not RECORDCOREs). To
 *      compare object titles, we can simply use the pszIcon fields.
 *      If we need the actual SOM objects, we can use the
 *      WPS's OBJECT_FROM_PREC macro.
 *
 *      Most of these use WinCompareString instead of strcmp so that
 *      PM's country settings are respected.
 *
 *@@header "shared\cnrsort.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSSEMAPHORES

#define INCL_WINCOUNTRY
#define INCL_WINSTDCNR
#include <os2.h>

#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

#include "shared\cnrsort.h"
#include "shared\kernel.h"              // XWorkplace Kernel

#pragma hdrstop
#include <wpfolder.h>      // WPFolder
#include <wpshadow.h>      // WPShadow

/*
 *@@ fnCompareExt:
 *      container sort comparison function for
 *      sorting records by file name extension.
 *
 *      This one should work with any MINIRECORDCORE,
 *      not just Desktop objects.
 */

SHORT EXPENTRY fnCompareExt(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    if ((pmrc1->pszIcon) && (pmrc2->pszIcon))
    {
        // find last dot char in icon titles;
        // strrchr finds LAST occurence (as opposed to strchr)
        PSZ pDot1 = strrchr(pmrc1->pszIcon, '.');
        PSZ pDot2 = strrchr(pmrc2->pszIcon, '.');
        if (pDot1 == NULL)
            if (pDot2 == NULL)
                // both titles have no extension: return names comparison
                switch (WinCompareStrings(G_habThread1,
                                          0,
                                          0,
                                          pmrc1->pszIcon,
                                          pmrc2->pszIcon,
                                          0))
                {
                    case WCS_LT: return (-1);
                    case WCS_GT: return (1);
                    default: return (0);
                }
            else
                // pmrc1 has no ext, but pmrc2 does:
                return (-1);
        else
            if (pDot2 == NULL)
                // pmrc1 has extension, but pmrc2 doesn't:
                return (1);

        // both records have extensions:
        // compare extensions
        switch (WinCompareStrings(G_habThread1,
                                  0,
                                  0,
                                  pDot1, pDot2,
                                  0))
        {
            case WCS_LT: return (-1);
            case WCS_GT: return (1);
            default:
                // same ext: compare names
                switch (WinCompareStrings(G_habThread1,
                                          0,
                                          0,
                                          pmrc1->pszIcon,
                                          pmrc2->pszIcon,
                                          0))
                {
                    case WCS_LT: return (-1);
                    case WCS_GT: return (1);
                }
        }
    }

    return (0);
}

/*
 *@@ fnCompareExtFoldersFirst:
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

SHORT EXPENTRY fnCompareExtFoldersFirst(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

    WPObject *pobjDeref1 = (_somIsA(pobj1, _WPShadow))
                                ? _wpQueryShadowedObject(pobj1, TRUE)
                                : pobj1;
    WPObject *pobjDeref2 = (_somIsA(pobj2, _WPShadow))
                                ? _wpQueryShadowedObject(pobj2, TRUE)
                                : pobj2;
    BOOL IsFldr1 = (pobjDeref1)
                      ? _somIsA(pobjDeref1, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders
    BOOL IsFldr2 = (pobjDeref2)
                      ? _somIsA(pobjDeref2, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders

    if (IsFldr1 && IsFldr2)
        // both are folders:
        // compare by name then
        return (fnCompareName(pmrc1, pmrc2, pStorage));
    else if (IsFldr1)
        return (-1);
    else if (IsFldr2)
        return (1);

    // none of the two are folders:
    // only then compare extensions
    return (fnCompareExt(pmrc1, pmrc2, pStorage));
}

/*
 *@@ fnCompareName:
 *      comparison func for sort by name.
 *
 *      This one should work with any MINIRECORDCORE,
 *      not just Desktop objects.
 */

SHORT EXPENTRY fnCompareName(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage; // to keep the compiler happy
    if ((pmrc1->pszIcon) && (pmrc2->pszIcon))
        switch (WinCompareStrings(G_habThread1,
                                  0,
                                  0,
                                  pmrc1->pszIcon,
                                  pmrc2->pszIcon,
                                  0))
        {
            case WCS_LT: return (-1);
            case WCS_GT: return (1);
        }

    return (0);
}

/*
 *@@ fnCompareNameFoldersFirst:
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

SHORT EXPENTRY fnCompareNameFoldersFirst(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

    WPObject *pobjDeref1 = (_somIsA(pobj1, _WPShadow))
                                ? _wpQueryShadowedObject(pobj1, TRUE)
                                : pobj1;
    WPObject *pobjDeref2 = (_somIsA(pobj2, _WPShadow))
                                ? _wpQueryShadowedObject(pobj2, TRUE)
                                : pobj2;
    BOOL IsFldr1 = (pobjDeref1)
                      ? _somIsA(pobjDeref1, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders
    BOOL IsFldr2 = (pobjDeref2)
                      ? _somIsA(pobjDeref2, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders

    if (IsFldr1 == IsFldr2)
        // both are folders, or both are NOT folders:
        // compare by name then
        return (fnCompareName(pmrc1, pmrc2, pStorage));
    else if (IsFldr1)
        return (-1);

    // else if IsFldr2)
    return (1);
}

/*
 *@@ fnCompareType:
 *      comparison func for sort by type (.TYPE EA)
 */

SHORT EXPENTRY fnCompareType(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    // get SOM objects from cnr record cores
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);
    PSZ pType1 = NULL, pType2 = NULL;
    if ((pobj1) && (pobj2))
    {
        if (_somIsA(pobj1, _WPFileSystem))
            pType1 = _wpQueryType(pobj1);
        if (_somIsA(pobj2, _WPFileSystem))
            pType2 = _wpQueryType(pobj2);

        if (pType1)
        {
            if (pType2)
            {
                switch (WinCompareStrings(G_habThread1,
                                          0,
                                          0,
                                          pType1,
                                          pType2,
                                          0))
                {
                    case WCS_LT: return (-1);
                    case WCS_GT: return (1);
                }
            }
            else
                // obj1 has type, obj2 has not
                return (-1);
        }
        else
            if (pType2)
                // obj1 has NO type, but obj2 does
                return (1);

    }

    return (0);
}

/*
 *@@ fnCompareTypeFoldersFirst:
 *      comparison func for sort by type (.TYPE EA),
 *      but folders first.
 */

SHORT EXPENTRY fnCompareTypeFoldersFirst(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

    WPObject *pobjDeref1 = (_somIsA(pobj1, _WPShadow))
                                ? _wpQueryShadowedObject(pobj1, TRUE)
                                : pobj1;
    WPObject *pobjDeref2 = (_somIsA(pobj2, _WPShadow))
                                ? _wpQueryShadowedObject(pobj2, TRUE)
                                : pobj2;
    BOOL IsFldr1 = (pobjDeref1)
                      ? _somIsA(pobjDeref1, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders
    BOOL IsFldr2 = (pobjDeref2)
                      ? _somIsA(pobjDeref2, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders

    if (IsFldr1 && IsFldr2)
        // both are folders:
        // compare by name then
        return (fnCompareName(pmrc1, pmrc2, pStorage));
    else if (IsFldr1)
        return (-1);
    else if (IsFldr2)
        return (1);

    // none of the two are folders:
    // only then compare extensions
    return (fnCompareType(pmrc1, pmrc2, pStorage));
}

/*
 *@@ fnCompareClass:
 *      comparison func for sort by object class
 */

SHORT EXPENTRY fnCompareClass(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    // get SOM objects from cnr record cores
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

    PSZ pszClass1 = _wpclsQueryTitle(_somGetClass(pobj1));
    PSZ pszClass2 = _wpclsQueryTitle(_somGetClass(pobj2));

    if ((pszClass1) && (pszClass2))
        switch (WinCompareStrings(G_habThread1,
                                  0,
                                  0,
                                  pszClass1,
                                  pszClass2,
                                  0))
        {
            case WCS_LT: return (-1);
            case WCS_GT: return (1);
        }

    return (0);
}

/*
 *@@ fnCompareClassFoldersFirst:
 *      comparison func for sort by type (.TYPE EA),
 *      but folders first.
 */

SHORT EXPENTRY fnCompareClassFoldersFirst(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
    WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

    WPObject *pobjDeref1 = (_somIsA(pobj1, _WPShadow))
                                ? _wpQueryShadowedObject(pobj1, TRUE)
                                : pobj1;
    WPObject *pobjDeref2 = (_somIsA(pobj2, _WPShadow))
                                ? _wpQueryShadowedObject(pobj2, TRUE)
                                : pobj2;
    BOOL IsFldr1 = (pobjDeref1)
                      ? _somIsA(pobjDeref1, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders
    BOOL IsFldr2 = (pobjDeref2)
                      ? _somIsA(pobjDeref2, _WPFolder)
                      : FALSE;      // treat broken shadows as non-folders

    if (IsFldr1 && IsFldr2)
        // both are folders:
        // compare by name then
        return (fnCompareName(pmrc1, pmrc2, pStorage));
    else if (IsFldr1)
        return (-1);
    else if (IsFldr2)
        return (1);

    // none of the two are folders:
    // only then compare extensions
    return (fnCompareClass(pmrc1, pmrc2, pStorage));
}

/*
 *@@ fnCompareRealName:
 *      comparison func for sort by real name.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareRealName(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage; // to keep the compiler happy
    if ((pmrc1) && (pmrc2))
        if ((pmrc1->pszIcon) && (pmrc2->pszIcon))
        {
            // get SOM objects from cnr record cores
            WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
            WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

            // the following defaults sort objects last that
            // have no real name (non-file-system objects)
            CHAR sz1[CCHMAXPATH] = {'\255', '\0'},
                 sz2[CCHMAXPATH] = {'\255', '\0'};

            if ((pobj1) && (pobj2))
            {
                if (_somIsA(pobj1, _WPFileSystem))
                    _wpQueryFilename(pobj1, sz1, FALSE);
                if (_somIsA(pobj2, _WPFileSystem))
                    _wpQueryFilename(pobj2, sz2, FALSE);

                switch (WinCompareStrings(G_habThread1,
                                          0,
                                          0,
                    sz1, sz2, 0))
                {
                    case WCS_LT: return (-1);
                    case WCS_GT: return (1);
                }
             }
        }
    return (0);
}

/*
 *@@ fnCompareSize:
 *      comparison func for sort by size.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareSize(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage; // to keep the compiler happy
    if ((pmrc1) && (pmrc2))
    {
        // get SOM objects from cnr record cores
        WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
        WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);
        ULONG ul1 = 0, ul2 = 0;
        if ((pobj1) && (pobj2))
        {
            if (_somIsA(pobj1, _WPFileSystem))
                ul1 = _wpQueryFileSize(pobj1);
            if (_somIsA(pobj2, _WPFileSystem))
                ul2 = _wpQueryFileSize(pobj2);

            if (ul1 < ul2)
                return (1);
            else if (ul1 > ul2)
                return (-1);
         }
    }
    return (0);
}

/*
 *@@ fnCompareCommonDate:
 *      common comparison func for sort by date functions below;
 *      ulWhat must be:
 *      --  1: compare write date
 *      --  2: compare access date
 *      --  3: compare creation date
 *
 *      This gets called by fnCompareLastWriteDate,
 *      fnCompareLastAccessDate, and fnCompareCreationDate
 *      and should not be specified as a sort function.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareCommonDate(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2,
                                   ULONG ulWhat)
{
    if ((pmrc1) && (pmrc2))
    {
        // get SOM objects from cnr record cores
        WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1);
        WPObject *pobj2 = OBJECT_FROM_PREC(pmrc2);

        // comparison is done using string comparison;
        // these are the defaults for non-WPFileSystem
        // objects, which will be sorted last
        CHAR sz1[30] = "9999.99.99 99:99:99",
             sz2[30] = "9999.99.99 99:99:99";
                     // year mm dd hh mm ss
        FDATE fdate;
        FTIME ftime;
        if ((pobj1) && (pobj2))
        {
            if (_somIsA(pobj1, _WPFileSystem))
            {
                switch (ulWhat)
                {
                    case 1: _wpQueryLastWrite(pobj1, &fdate, &ftime); break;
                    case 2: _wpQueryLastAccess(pobj1, &fdate, &ftime); break;
                    case 3: _wpQueryCreation(pobj1, &fdate, &ftime); break;
                }

                sprintf(sz1, "%04d.%02d.%02d %02d:%02d:%02d",
                        ((fdate.year)+1980),
                        fdate.month,
                        fdate.day,
                        ftime.hours,
                        ftime.minutes,
                        (ftime.twosecs * 2));
            }

            if (_somIsA(pobj2, _WPFileSystem))
            {
                switch (ulWhat)
                {
                    case 1: _wpQueryLastWrite(pobj2, &fdate, &ftime); break;
                    case 2: _wpQueryLastAccess(pobj2, &fdate, &ftime); break;
                    case 3: _wpQueryCreation(pobj2, &fdate, &ftime); break;
                }

                sprintf(sz2, "%04d.%02d.%02d %02d:%02d:%02d",
                        ((fdate.year)+1980),
                        fdate.month,
                        fdate.day,
                        ftime.hours,
                        ftime.minutes,
                        (ftime.twosecs * 2));
            }

            // printf("%s -- %s\n", sz1, sz2);
            switch (WinCompareStrings(G_habThread1, 0, 0,
                sz1, sz2, 0))
            {
                case WCS_LT: return (-1);
                case WCS_GT: return (1);
            }
        }
    }
    return (0);
}

/*
 *@@ fnCompareLastWriteDate:
 *      comparison func for sort by write date.
 *      This calls fnCompareCommonDate.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareLastWriteDate(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage;
    return (fnCompareCommonDate(pmrc1, pmrc2, 1));
}

/*
 *@@ fnCompareLastAccessDate:
 *      comparison func for sort by last access date.
 *      This calls fnCompareCommonDate.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareLastAccessDate(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage;
    return (fnCompareCommonDate(pmrc1, pmrc2, 2));
}

/*
 *@@ fnCompareCreationDate:
 *      comparison func for sort by creation date.
 *      This calls fnCompareCommonDate.
 *
 *      No longer used with the new sorting (V0.9.12),
 *      but we'll leave this here for now.
 */

SHORT EXPENTRY fnCompareCreationDate(PMINIRECORDCORE pmrc1, PMINIRECORDCORE pmrc2, PVOID pStorage)
{
    pStorage = pStorage;
    return (fnCompareCommonDate(pmrc1, pmrc2, 3));
}


