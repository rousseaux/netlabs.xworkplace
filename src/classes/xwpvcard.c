
/*
 *@@sourcefile xwpvcard.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPVCard (WPDataFile subclass)
 *
 *      XWPVCard is a special WPDataFile subclass for vCard files
 *      which is automatically assigned for each file that has the
 *      *.vcf file extension.
 *
 *      Installation of this class is optional.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 *@@somclass XWPVCard xvc_
 *@@somclass M_XWPVCard xvcM_
 */

/*
 *      Copyright (C) 2002 Ulrich M�ller.
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

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpvcard_Source
#define SOM_Module_xwpvcard_Source
#endif
#define XWPVCard_Class_Source
#define M_XWPVCard_Class_Source

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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xwpvcard.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   vCard parser
 *
 ********************************************************************/

typedef struct _PROPERTY
{
    XSTRING     strProperty,
                strParameters,
                strValue;

    PLINKLIST   pllSubList;
        // if != NULL, nested PROPERTY structs
} PROPERTY, *PPROPERTY;

/*
 *@@ vcfFree:
 *
 */

APIRET vcfFree(PLINKLIST *ppll)
{
    PLINKLIST pll;
    if (    (!ppll)
         || (!(pll = *ppll))
       )
        return ERROR_INVALID_PARAMETER;
    else
    {
        PLISTNODE pNode = lstQueryFirstNode(pll);
        while (pNode)
        {
            PPROPERTY pProp = (PPROPERTY)pNode->pItemData;
            xstrClear(&pProp->strProperty);
            xstrClear(&pProp->strParameters);
            xstrClear(&pProp->strValue);

            pNode = pNode->pNext;
        }

        lstFree(ppll);
    }

    return NO_ERROR;
}

/*
 *@@ vcfFindProperty:
 *
 *      A property is the definition of an individual attribute describing
 *      the vCard. A property takes the following form:
 +
 +          PropertyName [';' PropertyParameters] ':'  PropertyValue
 +
 *      as shown in the following example:
 +
 +          TEL;HOME:+1-919-555-1234
 +
 *      A property takes the form of one or more lines of text. The
 *      specification of property names and property parameters is
 *      case insensitive.
 *
 *      The property name can be one of a set of pre-defined strings.
 *      The property name, along with an optional grouping label,
 *      must appear as the first characters on a line.
 *      In the previous example, "TEL" is the name of the Telephone
 *      Number property.
 *
 *      Property values are specified as strings. In the previous
 *      example, "+1-919-555-1234" is the formatted value for the
 *      Telephone Number property.
 *
 *      A property value can be further qualified with a property
 *      parameter expression. Property parameter expressions are
 *      delimited from the property name with a semicolon.
 *      A semicolon in a property parameter value must be escaped
 *      with a backslash character. The property parameter expressions
 *      are specified as either a name=value or a value string. The
 *      value string can be specified alone in those cases where the
 *      value is unambiguous. For example a complete property parameter
 *      specification might be:
 +
 +      NOTE;ENCODING=QUOTED-PRINTABLE:Don't remember to order Girl=
 +          Scout cookies from Stacey today!
 +
 *      A valid short version of the same property parameter
 *      specification might be:
 +
 +      NOTE;QUOTED-PRINTABLE:Don t remember to order Girl=
 +          Scout cookies from Stacey today!
 *
 *      Continuation across several lines is possible by starting
 *      continuation lines with spaces. During parsing, any sequence
 *      of CRLF followed immediately by spaces is considered a
 *      continuation and will be removed in the returned value.
 *
 *      Standard properties:
 *
 *      --  FN: formatted name (what is displayed as the name).
 *
 *      --  N: structured name (family name;
 *                              given name;
 *                              addtl. names;
 *                              name prefix;
 *                              name suffix)
 *
 *          e.g.    N:Public;John;Quinlan;Mr.;Esq.
 *                  N:Veni, Vidi, Vici;The Restaurant.
 *
 *      --  PHOTO: photo of vCard's owner
 *
 +              PHOTO;VALUE=URL:file:///jqpublic.gif
 +
 +              PHOTO;ENCODING=BASE64;TYPE=GIF:
 +                  R0lGODdhfgA4AOYAAAAAAK+vr62trVIxa6WlpZ+fnzEpCEpzlAha/0Kc74+PjyGM
 +                  SuecKRhrtX9/fzExORBSjCEYCGtra2NjYyF7nDGE50JrhAg51qWtOTl7vee1MWu1
 +                  50o5e3PO/3sxcwAx/4R7GBgQOcDAwFoAQt61hJyMGHuUSpRKIf8A/wAY54yMjHtz
 *
 *      --  BDAY: birthday
 +
 +              BDAY:19950415
 +
 +              BDAY:1995-04-15
 *
 *      --  ADR: delivery address (compound: Post Office Address;
 *                                           Extended Address;
 *                                           Street;
 *                                           Locality;
 *                                           Region;
 *                                           Postal Code;
 *                                           Country)
 +
 +              ADR;DOM;HOME:P.O. Box 101;Suite 101;123 Main Street;Any Town;CA;91921-1234;
 *
 *      --  LABEL: delivery label (formatted)
 *
 *      --  TEL: telephone
 *
 *      --  EMAIL
 *
 *      --  MAILER: email software used by individual
 *
 *      --  TZ: timezone
 *
 *      --  GEO: geographic position
 *
 *      --  TITLE: job title etc.
 *
 *      --  ROLE: business role
 *
 *      --  LOGO: company logo or something
 *
 *      --  ORG: organization name
 *
 *      --  NOTE: a comment
 *
 *      --  REV: when vCard was last modified
 *
 *      --  SOUND: sound data
 *
 *      --  URL: where to find up-to-date information
 *
 *      --  UID: unique vCard identifier
 *
 *      --  VERSION: vCard version info (2.1)
 *
 *      --  KEY: public key
 *
 *      --  X-*: extension
 */

APIRET vcfTokenize(PSZ *ppszInput,
                   PLINKLIST *ppllProperties)      // out: PROPERTY list items
{
    PSZ         pLineThis = *ppszInput;
    ULONG       cbPropertyName;
    APIRET      arc = NO_ERROR;
    PLINKLIST   pList;
    ULONG       ul = 0;
    PXSTRING    pstrPrevValue = NULL;

    if (    (!ppszInput)
         || (!(*ppszInput))
         || (!ppllProperties)
       )
        return (ERROR_INVALID_PARAMETER);

    pList = lstCreate(FALSE);

    while (TRUE)
    {
        PSZ     pNextEOL = strhFindEOL(pLineThis, NULL);    // never NULL

        if (*pLineThis == ' ')
        {
            // continuation from previous line:
            // append to previous value string, if we had one
            if (!pstrPrevValue)
            {
                arc = ERROR_BAD_FORMAT;
                break;
            }
            else
            {
                // skip the spaces
                PSZ p = pLineThis + 1;
                while (*p == ' ')
                    p++;
                if (*p != '\n')
                    // line not empty:
                    xstrcat(pstrPrevValue,
                            p - 1,      // add one space always!
                            pNextEOL - p + 1);
            }
        }
        else
        {
            if (!strnicmp(pLineThis,
                          "END:VCARD",
                          9))
                // end of this vCard:
                return NO_ERROR;
            else
            {
                PSZ pNextColon = strchr(pLineThis, ':');        // can be NULL
                if (!pNextColon)
                    return ERROR_BAD_FORMAT;

                if (    (pNextColon < pNextEOL)
                     && (*pLineThis != '\n')       // not empty line
                   )
                {
                    // colon before EOL: then we can look
                    PSZ     pNextSemicolon;
                    ULONG   cbPropertyNameThis,
                            cbLineThis = pNextEOL - pLineThis;
                    PPROPERTY pProp;

                    *pNextColon = '\0';
                    nlsUpper(pLineThis,
                             pNextColon - pLineThis);

                    if (pNextSemicolon = strchr(pLineThis, ';'))
                        // we have a parameter:
                        cbPropertyNameThis = pNextSemicolon - pLineThis;
                    else
                        cbPropertyNameThis = pNextColon - pLineThis;

                    if (pProp = NEW(PROPERTY))
                    {
                        ZERO(pProp);
                        xstrcpy(&pProp->strProperty,
                                pLineThis,
                                cbPropertyNameThis);

                        while (pNextSemicolon)
                        {
                            // @@todo we can have several
                            // PHOTO;VALUE=URL;TYPE=GIF:http://www.abc.com/dir_photos/my_photo.gif
                            xstrcpy(&pProp->strParameters,
                                    pNextSemicolon + 1,
                                    // up to colon
                                    pNextColon - pNextSemicolon - 1);
                            pNextSemicolon = strchr(pNextSemicolon + 1, ';');
                        }

                        // now for the data:
                        // store string pointer for line continuations
                        pstrPrevValue = &pProp->strValue;
                        // and copy initial value
                        xstrcpy(pstrPrevValue,
                                pNextColon + 1,
                                pNextEOL - pNextColon - 1);
                        _Pmpf(("prop %d: \"%s\" \"%s\" \"%s\" ddd",
                               ul++,
                               STRINGORNULL(pProp->strProperty.psz),
                               STRINGORNULL(pProp->strParameters.psz),
                               STRINGORNULL(pProp->strValue.psz)
                             ));

                        lstAppendItem(pList,
                                      pProp);
                    }

                    *pNextColon = ':';
                }
            }
        }

        if (!*pNextEOL)
            // no more lines:
            break;

        pLineThis = pNextEOL + 1;
    }

    if (arc)
        vcfFree(&pList);
    else
        *ppllProperties = pList;

    return (arc);
}

/*
 *@@ vcfRead:
 *
 */

APIRET vcfRead(PCSZ pcszFilename,
               PLINKLIST *ppllProperties)      // out: PROPERTY list items
{
    APIRET  arc;
    PSZ     pszData = NULL;
    ULONG   cbRead;
    if (!(arc = doshLoadTextFile(pcszFilename,
                                 &pszData,
                                 &cbRead)))
    {
        XSTRING str;
        PSZ p;
        xstrInitSet(&str, pszData);
        xstrConvertLineFormat(&str, CRLF2LF);
        p = str.psz;
        arc = vcfTokenize(&p,
                          ppllProperties);

        xstrClear(&str);
    }

    return (arc);
}

/* ******************************************************************
 *
 *   XWPVCard instance methods
 *
 ********************************************************************/

/*
 *@@ wpAddSettingsPages:
 *
 */

SOM_Scope BOOL  SOMLINK xvc_wpAddSettingsPages(XWPVCard *somSelf,
                                               HWND hwndNotebook)
{
    CHAR sz[CCHMAXPATH];
    /* XWPVCardData *somThis = XWPVCardGetData(somSelf); */
    XWPVCardMethodDebug("XWPVCard","xvc_wpAddSettingsPages");

    if (_wpQueryFilename(somSelf, sz, TRUE))
    {
        APIRET arc;
        PLINKLIST pll = NULL;
        if (!(arc = vcfRead(sz,
                            &pll)))
        {
            PLISTNODE pNode = lstQueryFirstNode(pll);
            ULONG ul = 0;
            while (pNode)
            {
                PPROPERTY pProp = (PPROPERTY)pNode->pItemData;

                pNode = pNode->pNext;
                ul++;
            }
            vcfFree(&pll);
        }
        else
            _Pmpf((__FUNCTION__ ": vcfRead returned %d", arc));
    }

    return (XWPVCard_parent_WPDataFile_wpAddSettingsPages(somSelf,
                                                          hwndNotebook));
}


/* ******************************************************************
 *
 *   XWPVCard class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 *
 */

SOM_Scope void  SOMLINK xvcM_wpclsInitData(M_XWPVCard *somSelf)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsInitData");

    M_XWPVCard_parent_M_WPDataFile_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXWPVCard);
}

/*
 *@@ wpclsCreateDefaultTemplates:
 *
 */

SOM_Scope BOOL  SOMLINK xvcM_wpclsCreateDefaultTemplates(M_XWPVCard *somSelf,
                                                         WPObject* Folder)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsCreateDefaultTemplates");

    return (M_XWPVCard_parent_M_WPDataFile_wpclsCreateDefaultTemplates(somSelf,
                                                                       Folder));
}

/*
 *@@ wpclsQueryDefaultView:
 *      this WPObject class method returns the default view for
 *      objects of a class.
 *      The way this works is that WPObject::wpQueryDefaultView
 *      apparently checks for whether an instance default view
 *      has been set by the user. If not, this class method gets
 *      called.
 *
 *      We return OPEN_SETTINGS for the vCard. This will only work
 *      for *.vcf files that have not been previously opened,
 *      unfortunately, because the WPS resets data file default
 *      views to 0x1000 internally and stores that with the
 *      instance data, apparently.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xvcM_wpclsQueryDefaultView(M_XWPVCard *somSelf)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsQueryDefaultView");

    return OPEN_SETTINGS;

    // return (M_XWPVCard_parent_M_WPDataFile_wpclsQueryDefaultView(somSelf));
}

/*
 *@@ wpclsQueryTitle:
 *
 */

SOM_Scope PSZ  SOMLINK xvcM_wpclsQueryTitle(M_XWPVCard *somSelf)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsQueryTitle");

    return "vCard";
}

/*
 *@@ wpclsQueryIconData:
 *
 */

SOM_Scope ULONG  SOMLINK xvcM_wpclsQueryIconData(M_XWPVCard *somSelf,
                                                 PICONINFO pIconInfo)
{
    HMODULE hmod;
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsQueryIconData");

    if (hmod = cmnQueryMainResModuleHandle())
    {
        if (pIconInfo)
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->hmod = hmod;
            pIconInfo->resid = ID_ICONXWPVCARD;
        }

        return sizeof(ICONINFO);
    }

    return (M_XWPVCard_parent_M_WPDataFile_wpclsQueryIconData(somSelf,
                                                              pIconInfo));
}


/*
 *@@ wpclsQueryInstanceFilter:
 *      this WPDataFile class method determines which file-system
 *      objects will be instances of a certain class according
 *      to a file filter.
 *
 *      We assign ourselves to "*.vcf" types here.
 */

SOM_Scope PSZ  SOMLINK xvcM_wpclsQueryInstanceFilter(M_XWPVCard *somSelf)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsQueryInstanceFilter");

    return "*.vcf";
}

/*
 *@@ wpclsQueryInstanceType:
 *      this WPDataFile class method determines which file-system
 *      objects will be instances of a certain class according
 *      to a file type.
 *
 *      We assign ourselves to the "vCard" type here.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

SOM_Scope PSZ  SOMLINK xvcM_wpclsQueryInstanceType(M_XWPVCard *somSelf)
{
    /* M_XWPVCardData *somThis = M_XWPVCardGetData(somSelf); */
    M_XWPVCardMethodDebug("M_XWPVCard","xvcM_wpclsQueryInstanceType");

    return "vCard";
}

