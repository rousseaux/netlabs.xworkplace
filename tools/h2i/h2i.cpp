
/*
 *@@sourcefile h2i.cpp:
 *      the one and only source file for h2i.exe.
 *
 *      h2i is a replacement for HTML2IPF.CMD and now used
 *      by the XWorkplace and WarpIN makefiles to convert
 *      the various HTML sources in 001\ to IPF code, which
 *      can then be fed into IPFC.
 *
 *      Compared to HTML2IPF.CMD, h2i has the following
 *      advantages:
 *
 *      --  Even though it's fairly sloppy code, it's
 *          MAGNITUDES faster.
 *
 *      --  It can process C include files to allow for
 *          character entity references. In those include
 *          files, only #define statements are evaluated.
 *          This can be used for
 *
 *      --  It supports a RESID attribute to the HTML
 *          tag to allow for setting a resid explicitly.
 *          Together with the include facility, this
 *          is very useful for the XWorkplace help file.
 *
 *      --  This also fixes some minor formatting bugs
 *          that HTML2IPF.CMD exhibited, mostly with
 *          lists and PRE sections.
 *
 *      Current limitations:
 *
 *      --  This does not automatically convert GIFs or
 *          JPEGs to OS/2 1.3 BMP files, like HTML2IPF.CMD
 *          does.
 *
 *      --  <HTML SUBLINKS> and <HTML NOSUBLINKS> are not
 *          yet supported.
 *
 *@@header "h2i.h"
 *@@added V0.9.13 (2001-06-23) [umoeller]
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define OS2EMX_PLAIN_CHAR

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <io.h>

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include "setup.h"
#include "bldlevel.h"

#include "helpers\dosh.h"
#include "helpers\linklist.h"
#include "helpers\standards.h"
#include "helpers\stringh.h"
#include "helpers\tree.h"
#include "helpers\xstring.h"

#include "h2i.h"

#pragma info (nocnv)

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

/*
 *@@ DEFINENODE:
 *      represents a #define from a C header.
 */

typedef struct _DEFINENODE
{
    TREE        Tree;
                        // Tree.ulKey has the (PSZ) identifier
    PSZ         pszValue;
                        // value without quotes

} DEFINENODE, *PDEFINENODE;

/*
 *@@ ARTICLETREENODE:
 *      represents one article to be worked on.
 *
 *      This comes from one HTML file each and
 *      will turn into an IPF article.
 */

typedef struct _ARTICLETREENODE
{
    TREE        Tree;
                        // Tree.ulKey has the (PSZ) filename

    // PSZ         pszFilename;        // the HTML file name, or, if URL,
                                    // the URL
    ULONG       ulHeaderLevel;      // IPF header level (1 if root file;
                                    // -1 if link for "Internet" section)

    ULONG       ulResID;            // the resid of this link

    BOOL        fProcessed;         // TRUE after this has been parsed;
                                    // if FALSE, everything below is
                                    // still undefined

    // after parsing, the following are valid:

    XSTRING     strIPF;             // mostly translated IPF source

    XSTRING     strTitle;

    struct _ARTICLETREENODE *pFirstReferencedFrom;

    // misc data from HTML source

    LONG        lGroup;
    PSZ         pszWidth,
                pszXPos;
    BOOL        fHidden;

    // sublinks, nosublinks

    BOOL        fWritten;          // TRUE after this article has been
                                   // written to the main buffer; this
                                   // is used when we sort out parent
                                   // articles so we won't write the
                                   // article twice

} ARTICLETREENODE, *PARTICLETREENODE;

#define LIST_UL             1
#define LIST_OL             2
#define LIST_DL             3

/*
 *@@ STATUS:
 *      parser status.
 */

typedef struct _STATUS
{
    ULONG       ulLineLength;

    BOOL        fInHead,
                fItalics,
                fBold,
                fCode,
                fUnderlined,
                fInPre,
                fInTable,

                fJustHadSpace,
                fNeedsP;

    ULONG       ulInLink;           // 0: not in <A block
                                    // 1: in regular <A HREF= block, must be closed
                                    // 2: in special <A AUTO= block, must not be closed

    LINKLIST    llListStack;        // linklist abused as a stack for list tags;
                                    // the list item is simply a ULONG with a
                                    // LISTFL_* value

    ULONG       ulDefinition;       // if inside a <DL>, this is 1 if last item was
                                    // a DD

    BOOL        fFatal;             // if error is returned and this is TRUE,
                                    // processing is stopped; otherwise the
                                    // error is considered a warning only
} STATUS, *PSTATUS;

TREE        *G_DefinesTreeRoot;
// LINKLIST    G_llDefines;
ULONG       G_cDefines = 0;
ULONG       G_ulReplacements = 0;
TREE        *G_LinkIDsTreeRoot;

LINKLIST    G_llFiles2Process;

ULONG       G_ulVerbosity = 1;

BOOL        G_fNoMoveToRoot = FALSE;

/* ******************************************************************
 *
 *   misc
 *
 ********************************************************************/

/*
 *@@ Error:
 *
 *      ulCategory is:
 *
 *      --  0: command line syntax error.
 *
 *      --  1: HTML syntax warning.
 *
 *      --  2: fatal error.
 */

VOID Error(ULONG ulCategory,
           const char *pcszFile,
           ULONG ulLine,
           const char *pcszFunction,
           const char *pcszFormat,     // in: format string (like with printf)
           ...)                        // in: additional stuff (like with printf)
{
    if (G_ulVerbosity <= 1)
        printf("\n");
    if (G_ulVerbosity > 2)
        printf("h2i (%s %u %s)",
               pcszFile,
               ulLine,
               pcszFunction);
    else
        printf("h2i");

    if (pcszFormat)
    {
        va_list     args;
        switch (ulCategory)
        {
            case 0: printf(": Error in command line syntax. "); break;
            case 1: printf(" HTML warning: "); break;
            case 2: printf(" error: "); break;
        }
        va_start(args, pcszFormat);
        vprintf(pcszFormat, args);
        va_end(args);
    }

    if (ulCategory == 0)
        printf("\nType 'h2i -h' for help.\n");
    else
        printf("\n");
}

/*
 *@@ Explain:
 *
 */

VOID Explain(const char *pcszFormat,     // in: format string (like with printf)
             ...)                        // in: additional stuff (like with printf)
{
    if (pcszFormat)
    {
        va_list     args;
        printf("h2i error: ");
        va_start(args, pcszFormat);
        vprintf(pcszFormat, args);
        va_end(args);
        printf("\n");
    }

    printf("h2i V"BLDLEVEL_VERSION" ("__DATE__") (C) 2001 Ulrich M”ller\n");
    printf("  Part of the XWorkplace package.\n");
    printf("  This is free software under the GNU General Public Licence (GPL).\n");
    printf("  Refer to the COPYING file in the XWorkplace installation dir for details.\n");
    printf("h2i (for 'html to ipf') translates a large bunch of HTML files into a\n");
    printf("single IPF file, which can then be fed into IBM's ipfc compiler.\n");
    printf("Usage: h2i [-i<include>]... [-v[0|1|2|3]] [-s] [-r] <root.htm>\n");
    printf("with:\n");
    printf("   <root.htm>  the HTML file to start with. This better link to other files.\n");
    printf("   -i<include> include a C header, whose #define statements are parsed\n");
    printf("               to allow for entity references (&ent;).\n");
    printf("   -v[0|1|2|3] specify verbosity level. If not specified, defaults to 1;\n");
    printf("               if -v only is specified, uses 2.\n");
    printf("   -s          show some statistics when done.\n");
    printf("   -r          show only <root.htm> at level 1 in the TOC; otherwise all\n");
    printf("               second-level files are moved to level 1 as well.\n");
}

/*
 *@@ xstrprintf:
 *
 */

ULONG xstrprintf(PXSTRING pxstr,
                 const char *pcszFormat,
                 ...)
{
    CHAR sz[1000];
    ULONG ul;
    va_list     args;
    va_start(args, pcszFormat);
    vsprintf(sz, pcszFormat, args);
    va_end(args);

    ul = strlen(sz);
    xstrcat(pxstr,
            sz,
            ul);
    return (ul);
}

/* ******************************************************************
 *
 *   Parser
 *
 ********************************************************************/

/*
 *@@ fnCompareStrings:
 *      tree comparison func (src\helpers\tree.c).
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

int TREEENTRY fnCompareStrings(ULONG ul1, ULONG ul2)
{
    return (strhicmp((const char *)ul1,
                     (const char *)ul2));
}

/*
 *@@ AddDefinition:
 *
 */

BOOL AddDefinition(PDEFINENODE p)
{
    // lstAppendItem(&G_llDefines, p);
    // return (TRUE);
    return (!treeInsert(&G_DefinesTreeRoot,
                        (TREE*)p,
                        fnCompareStrings));
}

/*
 *@@ FindDefinition:
 *
 */

PDEFINENODE FindDefinition(const char *pcszIdentifier)
{
    return ((PDEFINENODE)treeFind(G_DefinesTreeRoot,
                                  (ULONG)pcszIdentifier,
                                  fnCompareStrings));

    /* PLISTNODE pNode = lstQueryFirstNode(&G_llDefines);
    while (pNode)
    {
        PDEFINENODE p = (PDEFINENODE)pNode->pItemData;
        if (!strcmp(p->pszIdentifier, pcszIdentifier))
            return (p);

        pNode = pNode->pNext;
    }

    return (0); */
}

/*
 *@@ ResolveEntities:
 *
 */

APIRET ResolveEntities(PARTICLETREENODE pFile2Process,
                       PXSTRING pstrSource)
{
    if (G_cDefines)
    {
        // we have include files:
        PSZ p = pstrSource->psz;
        while (p = strchr(p, '&'))
        {
            PSZ p2 = strchr(p, ';');
            if (!p2)
                p++;
            else
            {
                *p2 = '\0';

                PDEFINENODE pDef = 0;
                if (pDef = FindDefinition(p + 1))
                {
                    if (G_ulVerbosity > 2)
                        printf("\n   found entity \"%s\" --> \"%s\"\n",
                               p + 1, pDef->pszValue);
                    *p2 = ';';
                    ULONG ulPos = (p - pstrSource->psz),
                          ulReplLen = strlen(pDef->pszValue);
                    xstrrpl(pstrSource,
                            // first ofs to replace:
                            ulPos,
                            // char count to replace:
                            (p2 + 1) - p,       // include ';'
                            // replacement string:
                            pDef->pszValue,
                            ulReplLen);
                    // pointer has changed, adjust source position
                    p = pstrSource->psz + ulPos; //  + ulReplLen;

                    G_ulReplacements++;
                }
                else
                {
                    // not found:
                    // if it's not one of the standards we'll
                    // replace later, complain
                    if (    (strcmp(p + 1, "gt"))
                         && (strcmp(p + 1, "lt"))
                         && (strcmp(p + 1, "amp"))
                       )
                    {
                        Error(1,
                              __FILE__, __LINE__, __FUNCTION__,
                              "Unknown entity \"%s;\" at line %d in file \"%s\".",
                              p,
                              strhCount(pstrSource->psz, '\n') + 1,
                              (PSZ)pFile2Process->Tree.ulKey);      // filename

                        /* if (G_ulVerbosity > 3)
                        {
                            printf("All definitions:\n");
                            PDEFINENODE pNode = (PDEFINENODE)treeFirst(G_DefinesTreeRoot);
                            while (pNode)
                            {
                                printf("  \"%s\" = \"%s\"\n",
                                       pNode->pszIdentifier,
                                       pNode->pszValue);

                                pNode = (PDEFINENODE)treeNext((TREE*)pNode);
                            }

                            exit(99);
                        } */
                    }

                    *p2 = ';';
                }

                p = p2 + 1;
            }
        }
    }
}

/*
 *@@ GetOrCreateLinkID:
 *      returns a link ID (resid) for the
 *      specified filename.
 *
 *      If this is called for the first time for
 *      this file name, a new ID is created and
 *      returned. Otherwise the existing one
 *      is returned.
 *
 *      Special flags for ulCurrentLevel:
 *
 *      --  0: create root article (will receive 1 then)
 *
 *      -- -1: do not create if it doesn't exist.
 *
 *      If a file is created, it will intially
 *      carry a resid of -1. If the file has a
 *      RESID attribute to the HTML tag, ParseFile
 *      will reset the resid then. Later we go
 *      through all files and check which ones
 *      are still -1 and assign random values then.
 */

PARTICLETREENODE GetOrCreateArticle(const char *pcszFilename,
                                    ULONG ulCurrentLevel,
                                    PARTICLETREENODE pParent)
{
    PARTICLETREENODE pMapping;

    if (pMapping = (PARTICLETREENODE)treeFind(G_LinkIDsTreeRoot,
                                              (ULONG)pcszFilename,
                                              fnCompareStrings))
    {
        // exists:
        return (pMapping);
    }
    else
        if (ulCurrentLevel != -1)
        {
            // create new one
            pMapping = NEW(ARTICLETREENODE);
            if (pMapping)
            {
                ZERO(pMapping);

                pMapping->Tree.ulKey = (ULONG)strhdup(pcszFilename);
                pMapping->ulHeaderLevel = ulCurrentLevel + 1;
                pMapping->ulResID = -1;         // for now
                pMapping->pFirstReferencedFrom = pParent;

                xstrInit(&pMapping->strIPF, 0);
                xstrInit(&pMapping->strTitle, 0);

                if (!treeInsert(&G_LinkIDsTreeRoot,
                                (TREE*)pMapping,
                                fnCompareStrings))
                {
                    // this is a new file... check if this exists
                    if (access(pcszFilename, 0))
                    {
                        // does not exist:
                        // probably a URL or something...
                        if (G_ulVerbosity > 1)
                            printf("  Warning: file \"%s\" was not found.\n"
                                   "  Referenced from file \"%s\".\n",
                                   (PSZ)pMapping->Tree.ulKey, // pszFilename,
                                   (pParent)
                                       ? (PSZ)pMapping->Tree.ulKey // pszFilename,
                                       : "none");
                        pMapping->ulHeaderLevel = -1; // special flag
                    }

                    // since this is new, add it to the list of
                    // things to be processed too
                    lstAppendItem(&G_llFiles2Process,
                                  pMapping);

                    return (pMapping);
                }
            }
        }

    return (0);
}

/*
 *@@ CheckP:
 *      adds a :p. if this was markes as due
 *      in STATUS. When we encounter <P> in HTML,
 *      we never insert :p. directly but only
 *      set the flag in STATUS so we can do
 *      additional checks, mostly for better
 *      <LI> formatting. This gets called from
 *      various locations before appending
 *      other character data.
 */

VOID CheckP(PXSTRING pxstrIPF,
            PSTATUS pstat)    // in/out: parser status
{
    if (pstat->fNeedsP)
    {
        // we just had a <P> tag previously:
        if (pstat->fInPre)
            xstrcatc(pxstrIPF, '\n');
        else
            xstrcat(pxstrIPF, "\n:p.\n", 0);
        pstat->ulLineLength = 0;
        pstat->fJustHadSpace = TRUE;
        pstat->fNeedsP = FALSE;
    }
}

/*
 *@@ PushList:
 *      pushes the specified list on the list stack.
 */

VOID PushList(PSTATUS pstat,    // in/out: parser status
              ULONG ulList)     // in: LIST_* flag
{
    lstAppendItem(&pstat->llListStack,
                  (PVOID)ulList);
}

/*
 *@@ CheckListTop:
 *      returns TRUE if the top item on the list
 *      stack matches the given LIST_* flag.
 */

BOOL CheckListTop(PSTATUS pstat,    // in: status
                  ULONG ulList)     // in: LIST_* flag
{
    PLISTNODE pNode = lstQueryLastNode(&pstat->llListStack);
    if (pNode)
        if ((ULONG)pNode->pItemData == ulList)
            return (TRUE);
    return (FALSE);
}

/*
 *@@ PopList:
 *      removes the top item from the list stack.
 */

VOID PopList(PSTATUS pstat) // in/out: parser status
{
    lstRemoveNode(&pstat->llListStack,
                  lstQueryLastNode(&pstat->llListStack));
}

/*
 *@@ HandleHTML:
 *
 */

PCSZ HandleHTML(PARTICLETREENODE pFile2Process,
                PSZ pszAttrs)
{
    if (pszAttrs)
    {
        PSZ pszAttrib;
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "GROUP",
                                        NULL))
        {
            pFile2Process->lGroup = atoi(pszAttrib);
            free(pszAttrib);
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "WIDTH",
                                        NULL))
        {
            pFile2Process->pszWidth = pszAttrib;
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "XPOS",
                                        NULL))
        {
            pFile2Process->pszXPos = pszAttrib;
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "HIDDEN",
                                        NULL))
        {
            pFile2Process->fHidden = TRUE;
            free(pszAttrib);
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "RESID",
                                        NULL))
        {
            pFile2Process->ulResID = atoi(pszAttrib);
            if (!pFile2Process->ulResID)
                return ("Invalid RESID in <HTML> tag.");
            free(pszAttrib);
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "SUBLINKS",
                                        NULL))
        {
            // @@todo
        }
        if (pszAttrib = strhGetTextAttr(pszAttrs,
                                        "NOSUBLINKS",
                                        NULL))
        {
            // @@todo
        }
    }

    return (0);
}

/*
 *@@ HandleTITLE:
 *
 */

PCSZ HandleTITLE(PARTICLETREENODE pFile2Process,
                 PSZ *ppSource,
                 PSZ pNextClose)
{
    // TITLE tag...
    // we need to remove whitespace from the
    // front and the tail, so play a bit here
    PSZ p3 = pNextClose + 1;
    while (    *p3
            && (    (*p3 == ' ')
                 || (*p3 == '\n')
                 || (*p3 == '\t')
               )
          )
        p3++;
    if (!*p3)
        return ("Incomplete <TITLE>.");
    // find closing title tag
    PSZ p4 = strstr(p3, "</TITLE>");
    if (!p4)
        return ("<TITLE> has no closing tag.");
    else
    {
        // from beginning of title, go backwards
        // until the last non-whitespace
        PSZ p5 = p4 - 1;
        while (    p5 > p3
                && (    (*p5 == ' ')
                     || (*p5 == '\n')
                     || (*p5 == '\t')
                   )
              )
            p5--;

        if (p5 > p3)
        {
            xstrcpy(&pFile2Process->strTitle,
                    p3,
                    p5 - p3 + 1);
            ResolveEntities(pFile2Process,
                            &pFile2Process->strTitle);
            *ppSource = p4 + 8;
        }
        else
            return ("Empty <TITLE> block.");
    }

    return (0);
}

/*
 *@@ HandleA:
 *
 */

PCSZ HandleA(PARTICLETREENODE pFile2Process,
             PXSTRING pxstrIPF,
             PSTATUS pstat,    // in/out: parser status
             PSZ pszAttrs)
{
    if (pstat->ulInLink)
        return ("Nested <A ...> blocks.");
    else
    {
        // check the attribute:
        // is it HREF?
        if (pszAttrs)
        {
            PSZ pszAttrib;
            if (pszAttrib = strhGetTextAttr(pszAttrs,
                                            "AUTO",
                                            NULL))
            {
                PARTICLETREENODE patn
                    = GetOrCreateArticle(pszAttrib,
                                         pFile2Process->ulHeaderLevel,
                                         pFile2Process);
                xstrprintf(pxstrIPF,
                           // hack in the @#!LINK@#! for now;
                           // this is later replaced with the
                           // resid in ParseFiles
                           ":link reftype=hd res=@#!LINK@#!%s@#!LINK@#! auto dependent.",
                           pszAttrib);
                pstat->ulInLink = 2;        // special, do not close
            }
            else if (pszAttrib = strhGetTextAttr(pszAttrs,
                                            "HREF",
                                            NULL))
            {
                if (strhistr(pszAttrib, ".INF"))
                {
                    // special INF cross-link support:
                    // convert all # into spaces
                    PSZ p3 = pszAttrib;
                    while (*p3)
                    {
                        if (*p3 == '#')
                            *p3 = ' ';
                        p3++;
                    }

                    xstrprintf(pxstrIPF,
                               ":link reftype=launch object='view.exe' data='%s'.",
                               pszAttrib);
                }
                else
                {
                    // no INF:
                    PARTICLETREENODE patn
                    = GetOrCreateArticle(pszAttrib,
                                         pFile2Process->ulHeaderLevel,
                                         pFile2Process);
                    xstrprintf(pxstrIPF,
                               // hack in the @#!LINK@#! string for now;
                               // this is later replaced with the
                               // resid in ParseFiles
                               ":link reftype=hd res=@#!LINK@#!%s@#!LINK@#!.",
                               pszAttrib);
                }

                pstat->ulInLink = 1;        // regular, do close

                free(pszAttrib);
            }
            else
                return ("Unknown attribute to <A> tag.");
        }
        else
            return ("<A> tag has no attributes.");
    }

    return (0);
}

/*
 *@@ HandleIMG:
 *
 */

PCSZ HandleIMG(PXSTRING pxstrIPF,
               PSTATUS pstat,    // in/out: parser status
               PSZ pszAttrs)
{
    PSZ pszAttrib;
    if (pszAttrib = strhGetTextAttr(pszAttrs,
                                    "SRC",
                                    NULL))
    {
        XSTRING str;
        xstrInitSet(&str, pszAttrib);
        PSZ p = strrchr(pszAttrib, '.');
        if (    (!p)
             || (strlen(p) < 3)
           )
            xstrReserve(&str, CCHMAXPATH);
        strcpy(p, ".bmp");

        if (access(str.psz, 0))
            // doesn't exist:
            Error(1,
                  __FILE__, __LINE__, __FUNCTION__,
                  "The bitmap file \"%s\" was not found.",
                  str.psz);

        xstrprintf(pxstrIPF,
                   ":artwork name='%s' align=left.",
                   str.psz);
        xstrClear(&str);

        pstat->fNeedsP = FALSE;
    }
    else
        return ("No SRC attribute to <IMG> tag.");

    return (0);
}

/*
 *@@ HandleTag:
 *      monster HTML tag handler. Gets called from
 *      ParseFile whenever a '<' character is found
 *      in the HTML sources.
 *
 *      Terrible spaghetti, but works for now.
 */

const char *HandleTag(PARTICLETREENODE pFile2Process,
                      PSZ *ppSource,    // in: points to '<' char,
                                        // out: should point to '>' char
                      PSTATUS pstat)    // in/out: parser status
{
    const char *pcszError = NULL;
    PXSTRING pxstrIPF = &pFile2Process->strIPF;

    PSZ pStartOfTagName = *ppSource + 1;

    // is this a comment?
    if (!strncmp(pStartOfTagName, "!--", 3))
    {
        // start of comment:
        // find end of comment
        PSZ pEnd = strstr(pStartOfTagName + 3, "-->");
        if (pEnd)
        {
            // found:
            // search on after end of comment
            *ppSource = pEnd + 2;
        }
        else
        {
            // end of comment not found:
            // stop formatting...
            pcszError = "Terminating comment not found.";
            pstat->fFatal = TRUE;
        }
    }
    else
    {
        // no comment:
        // find end of tag
        PSZ     p2 = pStartOfTagName,
                pNextClose = 0,     // receives first '>' after '<'
                pNextSpace = 0;     // receives first ' ' after '<'
        BOOL    fCont = TRUE;
        while (fCont)
        {
            switch (*p2)
            {
                case ' ':
                case '\r':
                case '\n':
                    // store first space after '<'
                    if (!pNextSpace)
                        pNextSpace = p2;
                    *p2 = ' ';
                break;

                case '>':   // end of tag found:
                    pNextClose = p2;
                    fCont = FALSE;
                break;

                case '<':
                    // another opening tag:
                    // that's an HTML error
                    pcszError = "Opening < within another tag found.";
                break;

                case 0:
                    fCont = FALSE;
                break;
            }
            p2++;
        }

        if (pNextClose)
        {
            // end of tag found:
            ULONG cbTag;
            PSZ pszAttrs = NULL;

            if ((pNextSpace) && (pNextSpace < pNextClose))
            {
                // we have attributes:
                cbTag = pNextSpace - pStartOfTagName;
                pszAttrs = strhSubstr(pNextSpace, pNextClose);
            }
            else
                cbTag = pNextClose - pStartOfTagName;

            if (!cbTag)
            {
                // happens if we have a "<>" in the text:
                // just insert the '<>' and go on, we have no tag here
                xstrcatc(pxstrIPF, *(*ppSource++));
                xstrcatc(pxstrIPF, *(*ppSource++));
                pstat->ulLineLength += 2;
            }
            else
            {
                // attributes come after this
                // and go up to pNextClose

                // add a null terminator so we can
                // use strcmp for testing for tags
                CHAR cSaved = *(pStartOfTagName + cbTag);
                *(pStartOfTagName + cbTag) = '\0';

                // printf("'%s' ", pStartOfTagName);

                // now handle tags
                if (!strcmp(pStartOfTagName, "P"))
                {
                    // mark this only for now since we don't
                    // know yet whether a <LI> comes next...
                    // if we do <P><LI>, we get huge spaces
                    pstat->fNeedsP = TRUE;
                }
                else if (!strcmp(pStartOfTagName, "BR"))
                {
                    if (!pstat->fInTable)
                    {
                        // IPF cannot handle .br in tables
                        xstrcat(pxstrIPF, "\n.br\n", 0);
                        pstat->ulLineLength = 0;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "I"))
                          || (!strcmp(pStartOfTagName, "EM"))
                        )
                {
                    if (pstat->fItalics)
                        pcszError = "Nested <I> or <EM> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":hp1.", 0);
                        pstat->fItalics = TRUE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "/I"))
                          || (!strcmp(pStartOfTagName, "/EM"))
                        )
                {
                    if (!pstat->fItalics)
                        pcszError = "Unrelated closing </I> or </EM> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":ehp1.", 0);
                        pstat->fItalics = FALSE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "B"))
                          || (!strcmp(pStartOfTagName, "STRONG"))
                        )
                {
                    if (pstat->fBold)
                        pcszError = "Nested <B> or <STRONG> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":hp2.", 0);
                        pstat->fBold = TRUE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "/B"))
                          || (!strcmp(pStartOfTagName, "/STRONG"))
                        )
                {
                    if (!pstat->fBold)
                        pcszError = "Unrelated closing </B> or </STRONG> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":ehp2.", 0);
                        pstat->fBold = FALSE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "U"))
                        )
                {
                    if (pstat->fUnderlined)
                        pcszError = "Nested <U> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":hp5.", 0);
                        pstat->fUnderlined = TRUE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "/U"))
                        )
                {
                    if (!pstat->fUnderlined)
                        pcszError = "Unrelated closing </U> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":ehp5.", 0);
                        pstat->fUnderlined = FALSE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "CODE"))
                          || (!strcmp(pStartOfTagName, "CITE"))
                          || (!strcmp(pStartOfTagName, "TT"))
                        )
                {
                    if (pstat->fCode)
                        pcszError = "Nested <CODE>, <CITE>, or <TT> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":font facename='Courier' size=18x12.", 0);
                        pstat->fCode = TRUE;
                    }
                }
                else if (    (!strcmp(pStartOfTagName, "/CODE"))
                          || (!strcmp(pStartOfTagName, "/CITE"))
                          || (!strcmp(pStartOfTagName, "/TT"))
                        )
                {
                    if (!pstat->fCode)
                        pcszError = "Unrelated closing </CODE>, </CITE>, or </TT> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, ":font facename=default size=0x0.", 0);
                        pstat->fCode = FALSE;
                    }
                }
                else if (!strcmp(pStartOfTagName, "A"))
                    pcszError = HandleA(pFile2Process,
                                        pxstrIPF,
                                        pstat,
                                        pszAttrs);
                else if (!strcmp(pStartOfTagName, "/A"))
                {
                    if (pstat->ulInLink)
                    {
                        if (pstat->ulInLink == 1)
                            xstrcat(pxstrIPF,
                                    ":elink.",
                                    0);
                        // otherwise 2: that's from A AUTO, do not close

                        pstat->ulInLink = 0;
                    }
                    else
                        pcszError = "Unrelated closing </A> tag.";
                }
                else if (!strcmp(pStartOfTagName, "UL"))
                {
                    xstrcat(pxstrIPF, "\n:ul compact.", 0);
                    PushList(pstat, LIST_UL);
                    pstat->ulLineLength = 0;
                }
                else if (!strcmp(pStartOfTagName, "/UL"))
                {
                    if (CheckListTop(pstat, LIST_UL))
                    {
                        xstrcat(pxstrIPF, "\n:eul.", 0);
                        PopList(pstat);
                        pstat->fNeedsP = TRUE;
                    }
                    else
                        pcszError = "Invalid </UL> nesting.";
                }
                else if (!strcmp(pStartOfTagName, "OL"))
                {
                    xstrcat(pxstrIPF, "\n:ol compact.", 0);
                    PushList(pstat, LIST_OL);
                    pstat->ulLineLength = 0;
                }
                else if (!strcmp(pStartOfTagName, "/OL"))
                {
                    if (CheckListTop(pstat, LIST_OL))
                    {
                        xstrcat(pxstrIPF, "\n:eol.", 0);
                        PopList(pstat);
                        pstat->fNeedsP = TRUE;
                    }
                    else
                        pcszError = "Invalid </OL> nesting.";
                }
                else if (!strcmp(pStartOfTagName, "LI"))
                {
                    if (    (CheckListTop(pstat, LIST_UL))
                         || (CheckListTop(pstat, LIST_OL))
                       )
                    {
                        if (pstat->fNeedsP)
                        {
                            // we just had a <P> previously:
                            // add a .br instead or we'll get huge
                            // whitespace
                            xstrcat(pxstrIPF, "\n.br\n", 0);
                            pstat->fNeedsP = FALSE;
                        }
                        xstrcat(pxstrIPF, ":li.\n", 0);
                        pstat->ulLineLength = 0;
                    }
                    else
                        pcszError = "<LI> outside list.";
                }
                else if (!strcmp(pStartOfTagName, "DL"))
                {
                    xstrcat(pxstrIPF, "\n:dl compact break=all.", 0);
                    PushList(pstat, LIST_DL);
                    pstat->ulLineLength = 0;
                }
                else if (!strcmp(pStartOfTagName, "DT"))
                {
                    if (!CheckListTop(pstat, LIST_DL))
                        pcszError = "<DT> outside of <DL>.";
                    else
                    {
                        xstrcat(pxstrIPF, "\n:dt.", 0);
                        pstat->ulLineLength = 0;
                        pstat->ulDefinition = 0;
                    }
                }
                else if (!strcmp(pStartOfTagName, "DD"))
                {
                    if (!CheckListTop(pstat, LIST_DL))
                        pcszError = "<DD> outside of <DL>.";
                    else
                    {
                        xstrcat(pxstrIPF, "\n:dd.", 0);
                        pstat->ulLineLength = 0;
                        pstat->ulDefinition = 1;
                    }
                }
                else if (!strcmp(pStartOfTagName, "/DL"))
                {
                    if (CheckListTop(pstat, LIST_DL))
                    {
                        if (!pstat->ulDefinition)
                            // if the list didn't stop with a DD last,
                            // we must add one or IPFC will choke
                            xstrcat(pxstrIPF, "\n:dd.", 0);

                        xstrcat(pxstrIPF, "\n:edl.", 0);
                        PopList(pstat);
                        pstat->fNeedsP = TRUE;
                        pstat->ulDefinition = 0;
                    }
                    else
                        pcszError = "Invalid </DL> nesting.";
                }
                else if (!strcmp(pStartOfTagName, "HTML"))
                    pcszError = HandleHTML(pFile2Process,
                               pszAttrs);
                else if (!strcmp(pStartOfTagName, "TITLE"))
                    pcszError = HandleTITLE(pFile2Process,
                                            ppSource,
                                            pNextClose);
                else if (!strcmp(pStartOfTagName, "HEAD"))
                    pstat->fInHead = TRUE;
                            // this flag starts skipping all characters
                else if (!strcmp(pStartOfTagName, "/HEAD"))
                    pstat->fInHead = FALSE;
                            // stop skipping all characters
                else if (!strcmp(pStartOfTagName, "PRE"))
                {
                    if (pstat->fInPre)
                        pcszError = "Nested <PRE> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, "\n:cgraphic.", 0);
                        pstat->fInPre = TRUE;
                    }
                }
                else if (!strcmp(pStartOfTagName, "/PRE"))
                {
                    if (!pstat->fInPre)
                        pcszError = "Unrelated closing </PRE> tag.";
                    else
                    {
                        xstrcat(pxstrIPF, "\n:ecgraphic.", 0);
                        pstat->fInPre = FALSE;
                        pstat->fNeedsP = TRUE;
                    }
                }
                else if (!strcmp(pStartOfTagName, "IMG"))
                    pcszError = HandleIMG(pxstrIPF,
                                          pstat,
                                          pszAttrs);
                else if (!strcmp(pStartOfTagName, "HR"))
                {
                    ULONG ul;
                    xstrcat(pxstrIPF, ":cgraphic.", 0);
                    for (ul = 0; ul < 80; ul++)
                        xstrcatc(pxstrIPF, '_');
                    xstrcat(pxstrIPF, ":ecgraphic.", 0);
                    pstat->fNeedsP = TRUE;
                }

                // restore char under null terminator
                *(pStartOfTagName + cbTag) = cSaved;

                // skip the tag, even if unknown
                *ppSource = pNextClose;

                if (pszAttrs)
                    free(pszAttrs);
            }
        } // end if (pNextClose)
        else
        {
            xstrcatc(pxstrIPF,
                     **ppSource);
            // no closing tag found:
            // just return, caller will try next char
            // (probably no tag anyway)
        }
    }

    return (pcszError);
}

/*
 *@@ ConvertEscapes:
 *
 */

const char *ConvertEscapes(PXSTRING pstr)
{
    const char *papszSources[] =
        {
            "&amp;",
            "&lt;",
            "&gt;"
        };
    const char *papszTargets[] =
        {
            "&amp.",
            "<",
            ">"
        };

    ULONG ul;
    for (ul = 0;
         ul < ARRAYITEMCOUNT(papszSources);
         ul++)
    {
        ULONG ulOfs = 0;
        while (xstrFindReplaceC(pstr,
                                &ulOfs,
                                papszSources[ul],
                                papszTargets[ul]))
            ;
    }
}

/*
 *@@ ParseFile:
 *      converts the HTML code in the buffer to IPF.
 *
 *      This is a complete conversion except:
 *
 *      --  automatic resids will be assigned later,
 *          unless this file has <HTML RESID=xxx> set;
 *
 *      --  the :hX. tag will not be added here
 *          (because it depends on sublinks).
 *
 *      This adds files to G_llFiles2Process if they
 *      are linked to from this file by calling
 *      GetOrCreateArticle.
 */

APIRET ParseFile(PARTICLETREENODE pFile2Process,
                 PSZ pszBuf)
{
    PSZ    p = pszBuf;
    PXSTRING pxstrIPF = &pFile2Process->strIPF;

    STATUS stat = {0};
    lstInit(&stat.llListStack, FALSE);
    stat.fJustHadSpace = TRUE;

    while (TRUE)
    {
        const char *pcszError = NULL;
        CHAR    c = *p;
        PSZ     pSaved = p;

        switch (c)
        {
            case '<':
                pcszError = HandleTag(pFile2Process,
                                      &p,
                                      &stat);
            break;

            case ':':
                if (!stat.fInHead)
                {
                    CheckP(pxstrIPF, &stat);
                    xstrcat(pxstrIPF, "&colon.", 7);
                    stat.ulLineLength += 7;
                }
            break;

            case '.':
                if (!stat.fInHead)
                {
                    CheckP(pxstrIPF, &stat);
                    xstrcat(pxstrIPF, "&per.", 5);
                    stat.ulLineLength += 5;
                }
            break;

            case '\r':
                // just skip this
            break;

            case '\n':
            case ' ':
                // throw out all line breaks and spaces
                // unless we're in PRE mode
                if (stat.fInPre)
                    xstrcatc(pxstrIPF,
                             c);
                else
                {
                    CheckP(pxstrIPF, &stat);
                    // add a space instead, if we didn't just have one
                    // dump duplicate spaces
                    if (!stat.fJustHadSpace)
                    {
                        // on spaces, check how long the line is already...
                        if (stat.ulLineLength > 50)
                        {
                            xstrcatc(pxstrIPF, '\n');
                            stat.ulLineLength = 0;
                        }
                        else
                        {
                            xstrcatc(pxstrIPF, ' ');
                            (stat.ulLineLength)++;
                        }
                        stat.fJustHadSpace = TRUE;
                    }
                }

            break;

            case '\0':
                // null terminator reached:
                // some safety checks
                if (stat.fInHead)
                    pcszError = "No closing </HEAD> tag found.";
            break;

            default:
                if (!stat.fInHead)
                {
                    CheckP(pxstrIPF, &stat);
                    xstrcatc(pxstrIPF,
                             c);
                    (stat.ulLineLength)++;
                    stat.fJustHadSpace = FALSE;
                }
        }

        if (pcszError)
        {
            // get the line count
            CHAR c = *pSaved;
            *pSaved = 0;
            Error(1,
                  __FILE__, __LINE__, __FUNCTION__,
                  "file \"%s\", line %d:\n   %s",
                  (PSZ)pFile2Process->Tree.ulKey,       // file name
                  strhCount(pszBuf, '\n') + 1,
                  pcszError);
            *pSaved = c;
            if (stat.fFatal)
                return (1);
            // otherwise continue
        }

        if (*p)
            p++;
        else
            break;
    }

    ConvertEscapes(&pFile2Process->strTitle);
    ConvertEscapes(pxstrIPF);

    return (0);
}

/* ******************************************************************
 *
 *   File processor
 *
 ********************************************************************/

/*
 *@@ AppendToMainBuffer:
 *
 */

VOID AppendToMainBuffer(PXSTRING pxstrIPF,
                        PARTICLETREENODE pFile2Process,
                        PXSTRING pstrExtras)       // temp buffer for speed
{
    if (pFile2Process->ulHeaderLevel == 1)
    {
        // root file:
        xstrprintf(pxstrIPF,
                   ":title.%s\n",
                   pFile2Process->strTitle.psz);
    }

    xstrprintf(pxstrIPF,
               "\n.* Source file: \"%s\"\n",
               (PSZ)pFile2Process->Tree.ulKey); // pszFilename);

    xstrcpy(pstrExtras, NULL, 0);
    if (pFile2Process->lGroup)
        xstrprintf(pstrExtras,
                   " group=%d",
                   pFile2Process->lGroup);
    if (pFile2Process->pszXPos)
        xstrprintf(pstrExtras,
                   " x=%s",
                   pFile2Process->pszXPos);
    if (pFile2Process->pszWidth)
        xstrprintf(pstrExtras,
                   " width=%s",
                   pFile2Process->pszWidth);
    if (pFile2Process->fHidden)
        xstrcpy(pstrExtras, " hide", 0);

    ULONG ulHeaderLevel = pFile2Process->ulHeaderLevel;
    // if this is not at root level already,
    // move down one level (unless -r is specified)
    if (!G_fNoMoveToRoot)
        if (ulHeaderLevel > 1)
            ulHeaderLevel--;

    xstrprintf(pxstrIPF,
               ":h%d res=%d%s.%s\n",
               ulHeaderLevel,
               pFile2Process->ulResID,
               (pstrExtras->ulLength)
                    ? pstrExtras->psz
                    : "",
               pFile2Process->strTitle.psz);
    xstrcat(pxstrIPF,
            ":p.\n",
            0);

    // and the rest of the converted file
    xstrcats(pxstrIPF,
             &pFile2Process->strIPF);

    pFile2Process->fWritten = TRUE;
}

/*
 *@@ DumpArticlesWithParent:
 *
 */

VOID DumpArticlesWithParent(PXSTRING pxstrIPF,
                            PARTICLETREENODE pParent,
                            PXSTRING pstrExtras)
{
    PLISTNODE pNode = lstQueryFirstNode(&G_llFiles2Process);
    while (pNode)
    {
        PARTICLETREENODE pFile2Process = (PARTICLETREENODE)pNode->pItemData;

        // rule out special links for now
        if (    (pFile2Process->ulHeaderLevel != -1)
             && (!pFile2Process->fWritten)
           )
        {
            if (pFile2Process->pFirstReferencedFrom == pParent)
            {
                if (G_ulVerbosity > 1)
                    printf("File \"%s\" has resid %d, %d bytes IPF\n",
                           (PSZ)pFile2Process->Tree.ulKey, // pszFilename,
                           pFile2Process->ulResID,
                           pFile2Process->strIPF.ulLength);

                AppendToMainBuffer(pxstrIPF,
                                   pFile2Process,
                                   pstrExtras);

                // recurse with this node
                DumpArticlesWithParent(pxstrIPF,
                                       pFile2Process,
                                       pstrExtras);
            }

            // build a linked list of articles which belong
            // under this article

            /* PLISTNODE pSubnode = lstQueryFirstNode(&G_llFiles2Process);
            while (pSubnode)
            {
                PARTICLETREENODE pSub = (PARTICLETREENODE)pSubnode->pItemData;

                if (    (pSub->pFirstReferencedFrom == pFile2Process)
                     && (pSub->ulHeaderLevel != -1)
                   )
                {
                    if (G_ulVerbosity > 1)
                        printf("   loop 2: Writing out %s\n",
                               pSub->pszFilename);
                    AppendToMainBuffer(pxstrIPF,
                                       pSub,
                                       &strExtras);
                        // marks the file as "written"
                }

                pSubnode = pSubnode->pNext;
            } */
        }

        // now for the next main node
        pNode = pNode->pNext;
    }
}

/*
 *@@ ProcessFiles:
 *      loops through all files.
 *
 *      When this is called (fro main()), G_llFiles2Process
 *      contains only the one file that was specified on
 *      the command line. This then calls ParseFile() on
 *      that file, which will add to the list.
 *
 *      After ParseFile() has been called for each such
 *      file then, we run several additional loops here
 *      for resid resolution and such.
 *
 *      Finally, in the last loop, the IPF code from all
 *      files is added to the one given XSTRING buffer,
 *      which will then be written out to disk by the
 *      caller (main()).
 */

APIRET ProcessFiles(PXSTRING pxstrIPF)           // out: one huge IPF file
{
    PLISTNODE pNode;
    APIRET arc = NO_ERROR;

    /*
     * loop 1:
     *
     */

    if (G_ulVerbosity > 1)
        printf("Reading source files...\n");

    // go thru the list of files to process and
    // translate them from HTML to IPF...
    // when this func gets called, this list
    // contains only the root file, but ParseFile
    // will add new files to the list, so we just
    // keep going
    XSTRING strSource;
    xstrInit(&strSource, 0);

    pNode = lstQueryFirstNode(&G_llFiles2Process);
    while (pNode)
    {
        PARTICLETREENODE pFile2Process = (PARTICLETREENODE)pNode->pItemData;

        // rule out special links for now
        if (pFile2Process->ulHeaderLevel != -1)
        {
            PSZ pszContents = NULL;

            if (G_ulVerbosity > 1)
            {
                printf("processing %s... ",
                       (PSZ)pFile2Process->Tree.ulKey); // pszFilename);
                fflush(stdout);
            }
            else if (G_ulVerbosity)
            {
                printf(".");
                fflush(stdout);
            }

            if (!(arc = doshLoadTextFile((PSZ)pFile2Process->Tree.ulKey, // pszFilename,
                                         &pszContents)))
            {
                xstrcpy(&strSource, pszContents, 0);
                xstrConvertLineFormat(&strSource, CRLF2LF);

                ResolveEntities(pFile2Process,
                                &strSource);

                // now go convert this buffer to IPF
                if (!(arc = ParseFile(pFile2Process,
                                      strSource.psz)))
                {
                }

                pFile2Process->fProcessed = TRUE;

                if (G_ulVerbosity > 1)
                    printf("\n");
            }
            else
            {
                Error(2,
                      __FILE__, __LINE__, __FUNCTION__,
                      "Error %d occured reading file \"%s\".",
                      arc,
                      (PSZ)pFile2Process->Tree.ulKey); // pszFilename);
                arc = 1;
            }
        }

        if (arc)
            break;

        // next file (ParseFile has added files to list)
        pNode = pNode->pNext;
    }

    if (!arc)
    {
        /*
         * loop 2:
         *
         */

        // go through the entire file list again
        // and assign an automatic resid if this
        // has not been set yet

        if (G_ulVerbosity > 1)
            printf("Assigning automatic resids...\n");

        ULONG ulNextResID = 10000;

        pNode = lstQueryFirstNode(&G_llFiles2Process);
        while (pNode)
        {
            PARTICLETREENODE pFile2Process = (PARTICLETREENODE)pNode->pItemData;

            // process special links too
            if (pFile2Process->ulResID == -1)
                pFile2Process->ulResID = ulNextResID++;

            pNode = pNode->pNext;
        }

        if (G_ulVerbosity > 1)
            printf("  Done, %d resids assigned (%d files had explicit resids)\n",
                   (ulNextResID - 10000),
                   lstCountItems(&G_llFiles2Process) - (ulNextResID - 10000));

        /*
         * loop 2:
         *
         */

        // go through the entire file list again
        // and in each IPF buffer, replace the
        // special link strings with the good resids,
        // which are known by now

        pNode = lstQueryFirstNode(&G_llFiles2Process);
        while (pNode)
        {
            PARTICLETREENODE pFile2Process = (PARTICLETREENODE)pNode->pItemData;

            // rule out special links for now
            if (pFile2Process->ulHeaderLevel != -1)
            {
                PSZ pStart = pFile2Process->strIPF.psz,
                    p;
                // now, find the special ugly link keys
                // we hacked in before; for each string
                // target filename now, find the resid
                while (p = (strstr(pStart,
                                   "@#!LINK@#!")))
                {
                    #define KEYLEN (sizeof("@#!LINK@#!") - 1)
                    PSZ p2 = strstr(p + 10, "@#!LINK@#!");
                    CHAR szResID[30];
                    CHAR cSaved = *p2;
                    *p2 = '\0';
                    PARTICLETREENODE pTarget = GetOrCreateArticle(p + KEYLEN,   // filename
                                                                  -1,   // do not create
                                                                  NULL);

                    if (!pTarget)
                        Error(2,
                              __FILE__, __LINE__, __FUNCTION__,
                              "Cannot resolve cross reference for \"%s\"\n",
                              p + KEYLEN);

                    *p2 = cSaved;

                    sprintf(szResID, "%d", pTarget->ulResID);
                    xstrrpl(&pFile2Process->strIPF,
                            // ofs of first char to replace:
                            p - pStart,
                            // count of chars to replace:
                            (p2 + KEYLEN) - p,
                            // replace this with the resid
                            szResID,
                            strlen(szResID));

                    p += 10;
                }
            }

            pNode = pNode->pNext;
        }

        XSTRING strExtras;
        xstrInit(&strExtras, 0);

        /*
         * loop 3:
         *
         */

        LINKLIST llSubarticles;
        lstInit(&llSubarticles, FALSE);

        // now dump out IPF into one huge buffer...
        // note, this loop will go thru all files again,
        // but since we have a second loop within which
        // marks files as written, this will effectively
        // only process the root items
        DumpArticlesWithParent(pxstrIPF,
                               NULL,       // root documents only
                               &strExtras);
                // this will recurse

        /*
         * loop 4:
         *
         */

        // add all the special links
        // to the bottom; these should be the only
        // unprocessed files, or something went wrong here
        pNode = lstQueryFirstNode(&G_llFiles2Process);
        BOOL fFirst = TRUE;
        XSTRING strEncode;
        xstrInit(&strEncode, 0);
        while (pNode)
        {
            PARTICLETREENODE pFile2Process = (PARTICLETREENODE)pNode->pItemData;

            // process only the special files now
            if (pFile2Process->ulHeaderLevel == -1)
            {
                if (fFirst)
                {
                    xstrcat(pxstrIPF,
                            ":h1 group=99 x=right width=30%.Resources on the Internet\n",
                            0);
                    xstrcat(pxstrIPF,
                            "This chapter contains all external links referenced in this "
                            "book.\nEach link contained herein is an Unified Resource "
                            "Locator (URL) to a certain location\non the Internet. "
                            "Simply double-click on one of them to launch Netscape\n"
                            "with the respective URL.\n",
                            0);
                    fFirst = FALSE;
                }

                // encode the strings again because STUPID ipf
                // gets confused otherwise
                xstrcpy(&strEncode,
                        (PSZ)pFile2Process->Tree.ulKey, // pszFilename,
                        0);
                ConvertEscapes(&strEncode);
                ULONG ulOfs = 0;
                while (xstrFindReplaceC(&strEncode,
                                        &ulOfs,
                                        ":",
                                        "&colon."))
                    ;

                xstrprintf(pxstrIPF,
                           ":h2 res=%d group=98 x=right y=bottom width=60%% height=40%%.%s\n",
                           pFile2Process->ulResID,
                           strEncode.psz);
                xstrprintf(pxstrIPF,
                           ":p.:lines align=center."
                                "\nClick below to launch Netscape with this URL&colon.\n"
                                ":p.:link reftype=launch object='netscape.exe' data='%s'.\n"
                                "%s\n"
                                ":elink.:elines.\n",
                           (PSZ)pFile2Process->Tree.ulKey, // pszFilename,
                           strEncode.psz);
            }
            else
                if (!pFile2Process->fProcessed)
                    Error(2,
                          __FILE__, __LINE__, __FUNCTION__,
                          "Strange, file \"%s\" wasn't processed.",
                          (PSZ)pFile2Process->Tree.ulKey); // pszFilename);

            pNode = pNode->pNext;
        }

        if (G_ulVerbosity > 1)
            printf("\nDone processing files.\n");
        else
            printf("\n");
    }

    return (arc);
}

/* ******************************************************************
 *
 *   main
 *
 ********************************************************************/

/*
 *@@ AddDefine:
 *
 */

BOOL AddDefine(const char *pcszDefine)      // in: first char after #define
{
    PCSZ    p = pcszDefine;
    PSZ     pszIdentifier = NULL,
            pszValue = NULL;

    /*
                    printf("testing %c%c%c%c%c%c%c%c%c%c\n",
                            *p,     *(p+1), *(p+2), *(p+3), *(p+4),
                            *(p+5), *(p+6), *(p+7), *(p+8), *(p+9));
    */

    while ((*p) && (!pszValue))
    {
        switch (*p)
        {
            // skip comments
            case '/':
            case '\r':
            case '\n':
                // forget it
                    // printf("      quitting\n");
                return (FALSE);

            case ' ':
            case '\t':
            break;

            case '"':
            {
                if (pszIdentifier)
                {
                    /* printf("testing %c%c%c%c%c%c%c%c%c%c\n",
                            *p,     *(p+1), *(p+2), *(p+3), *(p+4),
                            *(p+5), *(p+6), *(p+7), *(p+8), *(p+9)); */

                    // looks like a string constant...
                    PCSZ pEnd = p + 1;
                    while (*pEnd && !pszValue)
                    {
                        switch (*pEnd)
                        {
                            case '\r':
                            case '\n':
                                // forget it
                                return (FALSE);

                            case '"':
                                // looks goood!
                                pszValue = strhSubstr(p + 1,    // after quote
                                                      pEnd);
                                // printf("-->found \"%s\"\n", pszValue);
                            break;
                        }

                        if ((pEnd) && (*pEnd))
                            pEnd++;
                        else
                            break;
                    }
                }
                else
                    // quote before identifier: don't think so
                    return (FALSE);
            }
            break;

            default:
            {
                // regular character:
                // go to the next space
                // this must be the identifier then
    /*
    printf("testing %c%c%c%c%c\n",
            *p, *(p+1), *(p+2), *(p+3), *(p+4));
            */
                PCSZ pEnd = p;          // first non-space character
                while (    (*pEnd)
                        && (*pEnd != ' ')
                        && (*pEnd != '\t')
                        && (*pEnd != '\n')
                      )
                    pEnd++;
                if (    (*pEnd == ' ')
                     || (*pEnd == '\t')
                     || ((pszIdentifier) && (*pEnd == '\n'))
                   )
                {
                    // got it
                    if (!pszIdentifier)
                    {
                        pszIdentifier = strhSubstr(p, pEnd);
                                // printf("  got identifier \"%s\"\n", pszIdentifier);
                    }
                    else
                        pszValue = strhSubstr(p, pEnd);

                    // go on after this
                    p = pEnd;
                }
                else
                    // bull
                    return (FALSE);
            }
            break;
        }

        if ((p) && (*p))
            p++;
        else
            return (FALSE);
    }

    if (pszValue)
    {
        PDEFINENODE pMapping = NEW(DEFINENODE);
        pMapping->Tree.ulKey = (ULONG)pszIdentifier;
        pMapping->pszValue = pszValue;

        if (AddDefinition(pMapping))
        {
            if (G_ulVerbosity > 2)
                printf("  found #define \"%s\" \"%s\"\n",
                       pszIdentifier,
                       pszValue);
            G_cDefines++;
            return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ ParseCHeader:
 *
 */

APIRET ParseCHeader(const char *pcszHeaderFile,
                    PULONG pcDefines)
{
    PSZ pszContents;
    APIRET arc;
    ULONG cDefines = 0;
    if (!(arc = doshLoadTextFile(pcszHeaderFile,
                                 &pszContents)))
    {
        XSTRING strSource;
        xstrInitSet(&strSource, pszContents);
        xstrConvertLineFormat(&strSource, CRLF2LF);

        PSZ p = strSource.psz;

        while (TRUE)
        {
            switch (*p)
            {
                // skip comments
                case '/':
                    switch (*(p+1))
                    {
                        case '/':       // C++ comment
                            // go to the end of the line, below
                            p = strhFindEOL(p + 1, NULL);
                        break;

                        case '*':       // C comment
                            p = strstr(p+2, "*/");
                        break;

                    }
                break;

                case '#':
                    if (!strncmp(p + 1,
                                 "define",
                                 6))
                        if (AddDefine(p + 7))
                            cDefines++;

                    // go to the end of the line,
                    // there can't be anything else
                    p = strhFindEOL(p + 1, NULL);
                break;

                case ' ':
                case '\t':
                    // ignore
                    p++;
                    continue;
                break;
            }

            if (p && (*p))
                p++;
            else
                break;
        }

        xstrClear(&strSource);
    }

    if (!arc)
        *pcDefines = cDefines;

    return (arc);
}

/*
 *@@ main:
 *
 */

int main(int argc, char* argv[])
{
    int     rc = 0;
    CHAR    szRootFile[CCHMAXPATH] = "";

    BOOL    fShowStatistics = FALSE;

    treeInit(&G_LinkIDsTreeRoot);
    treeInit(&G_DefinesTreeRoot);
    // lstInit(&G_llDefines, FALSE);
    lstInit(&G_llFiles2Process, FALSE);

    LINKLIST llIncludes;
    lstInit(&llIncludes, TRUE);         // will hold plain -i filenames from strdup

    if (argc < 2)
    {
        Explain(NULL);
        rc = 999;
    }
    else
    {
        SHORT i = 0;
        while (    (i++ < argc - 1)
                && (!rc)
              )
        {
            if (argv[i][0] == '-')
            {
                // option found:
                SHORT i2;
                for (i2 = 1;
                     i2 < strlen(argv[i]) && (!rc);
                     i2++)
                {
                    CHAR cOption = argv[i][i2];
                    switch (cOption)
                    {
                        case 'h':
                        case '?':
                            Explain(NULL);
                            rc = 1;
                        break;

                        case 's':
                            fShowStatistics = TRUE;
                        break;

                        case 'v':
                            i2++;
                            switch (argv[i][i2])
                            {
                                case '0': G_ulVerbosity = 0; break;
                                case '1': G_ulVerbosity = 1; break;     // default also
                                case '3': G_ulVerbosity = 3; break;
                                case '4': G_ulVerbosity = 4; break;
                                default: G_ulVerbosity = 2;
                            }
                        break;

                        case 'i':
                            if (strlen(&argv[i][i2+1]))
                            {
                                lstAppendItem(&llIncludes,
                                              strdup(&argv[i][i2+1]));
                            }
                            else
                            {
                                Error(2,
                                      __FILE__, __LINE__, __FUNCTION__,
                                      "No filename specified with -i option.");
                                rc = 99;
                            }
                            i2 = 999999999;
                        break;

                        case 'r':
                            G_fNoMoveToRoot = TRUE;
                        break;

                        default:  // unknown option
                            Explain("Unknown option '%c'.",
                                    cOption);
                            rc = 999;
                        break;
                    }
                }
            } // end if (argv[i][0] == '-')
            else
            {
                /*
                 * collect file names:
                 *
                 */

                if (!szRootFile[0])
                    strcpy(szRootFile, argv[i]);
                else
                {
                    Explain("More than one root file specified.");
                    rc = 999;
                }
            } // end elseif (argv[i][0] == '-')
        }
    }

    if (!szRootFile[0] && !rc)
    {
        Explain("You have not specified any input file.");
        rc = 999;
    }

    if (!rc)
    {
        // parse includes, if any
        PLISTNODE pNode = lstQueryFirstNode(&llIncludes);
        while (pNode)
        {
            PSZ pszInclude = (PSZ)pNode->pItemData;
            ULONG cDefines = 0;
            if (!(rc = ParseCHeader(pszInclude,
                                    &cDefines)))
            {
                if (G_ulVerbosity)
                    printf("Found %d valid #define's in \"%s\".\n",
                           cDefines,
                           pszInclude);
            }
            else
                 Error(2,
                       __FILE__, __LINE__, __FUNCTION__,
                       "Error %d opening include file \"%s\".",
                       rc,
                       pszInclude);

            pNode = pNode->pNext;
        }


        XSTRING str;
        xstrInit(&str, 100*1000);

        xstrcpy(&str,
                ":userdoc.\n",
                0);
        xstrcat(&str,
                ":docprof toc=12345.\n",   // let heading levels 1-5 appear in TOC
                0);

        GetOrCreateArticle(szRootFile,
                           0,             // current nesting level will be 1 then
                           NULL);

        rc = ProcessFiles(&str);

        xstrcat(&str,
                ":euserdoc.",
                0);
        if (!rc)
        {
            CHAR szOutputFile[CCHMAXPATH];
            strcpy(szOutputFile, szRootFile);
            PSZ p = strrchr(szOutputFile, '.');
            if (p)
                strcpy(p, ".ipf");
            else
                strcat(szOutputFile, ".ipf");

            ULONG cbWritten = 0;

            XSTRING str2;
            xstrInit(&str2, str.ulLength * 2 / 3);

            p = str.psz;
            CHAR c;
            while (c = *p++)
            {
                if (c == '\n')
                    xstrcatc(&str2, '\r');
                xstrcatc(&str2, c);
            }
            xstrcatc(&str2, '\0');

            if (rc = doshWriteTextFile(szOutputFile,
                                       str2.psz,
                                       &cbWritten,
                                       NULL))
                Error(2,
                      __FILE__, __LINE__, __FUNCTION__,
                      "Error %d writing output file \"%s\".",
                      rc,
                      szOutputFile);
            else
                if ((fShowStatistics) || (G_ulVerbosity > 1))
                {
                    printf("%d HTML files were processed\n",
                           lstCountItems(&G_llFiles2Process));
                    printf("%d #defines were active\n",
                           G_cDefines);
                    printf("%d character entities were replaced\n",
                           G_ulReplacements);
                    printf("Output file \"%s\" successfully written, %d bytes\n",
                            szOutputFile,
                            cbWritten);
                }
        }
    }

    return (rc);
}

