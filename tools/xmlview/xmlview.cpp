
/*
 *@@sourcefile xmlview.cpp:
 *
 *
 *@@header "xmlview.h"
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

#include "setup.h"

#include "xwpapi.h"

#include "expat\expat.h"

#include "helpers\cnrh.h"
#include "helpers\datetime.h"           // date/time helper routines
#include "helpers\dialog.h"
#include "helpers\dosh.h"
#include "helpers\except.h"
#include "helpers\linklist.h"
#include "helpers\stringh.h"
#include "helpers\threads.h"
#include "helpers\tree.h"
#include "helpers\winh.h"
#include "helpers\xstring.h"
#include "helpers\xml.h"

#include "xmlview.h"

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HWND        G_hwndMain = NULLHANDLE;
HWND        G_hwndStatusBar = NULLHANDLE;
HWND        G_hwndCnr = NULLHANDLE;

PFNWP       G_pfnwpCnrOrig = NULL,
            G_fnwpFrameOrig = NULL;

const char  *INIAPP              = "XWorkplace:XMLView";
const char  *INIKEY_MAINWINPOS   = "HandleFixWinPos";
const char  *INIKEY_OPENDLG      = "FileOpenDlgPath";

const char  *APPTITLE            = "xmlview";

/*
 *@@ NODERECORD:
 *
 */

typedef struct _NODERECORD
{
    RECORDCORE recc;

} NODERECORD, *PNODERECORD;

/*
 *@@ XMLFILE:
 *
 *@@added V0.9.9 (2001-02-10) [umoeller]
 */

typedef struct _XMLFILE
{
    XSTRING         strContents;
                        // contents of the XML document

    PXMLDOM         pDom;

} XMLFILE, *PXMLFILE;

PXMLFILE     G_pLoadedFile = NULL;

/* ******************************************************************
 *
 *   XML load
 *
 ********************************************************************/

/*
 *@@ winhSetWindowText:
 *      sets a window's text to a variable text. This is
 *      just like WinSetWindowText, but supports printf()
 *      formatting.
 *
 *      This is limited to 2000 characters.
 *
 *@@added V0.9.9 (2001-02-10) [umoeller]
 */

VOID winhSetWindowText(HWND hwnd,
                       const char *pcszFormat,     // in: format string (as with printf)
                       ...)                        // in: additional stuff (as with printf)
{
    va_list     args;
    CHAR        sz[2000];

    va_start(args, pcszFormat);
    vsprintf(sz, pcszFormat, args);
    va_end(args);

    WinSetWindowText(hwnd, sz);
}

/*
 *@@ XwpFileDlg:
 *
 */

HWND XwpFileDlg(HWND hwndO,
                HWND hwndP,
                PFILEDLG pfd)
{
    HWND hwndReturn = NULLHANDLE;

    if (pfd)
    {
        // check if XWP is running; if so, a block of
        // named shared memory must exist
        PXWPGLOBALSHARED pXwpGlobalShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pXwpGlobalShared,
                                          SHMEM_XWPGLOBAL,
                                          PAG_READ | PAG_WRITE);
        if (arc == NO_ERROR)
        {
            // we can get the shared memory:
            PID pidWPS = 0;
            TID tidWPS = 0;

            // create temporary object window for notifications from XFLDR.DLL
            HWND hwndNotify = winhCreateObjectWindow(WC_STATIC, NULL);
            HAB hab = WinQueryAnchorBlock(hwndNotify);

            // check if the thread-1 object in XFLDR.DLL window is running
            if (    (pXwpGlobalShared->hwndThread1Object)
                 && (WinIsWindow(hab, pXwpGlobalShared->hwndThread1Object))
                 && (WinQueryWindowProcess(pXwpGlobalShared->hwndThread1Object,
                                           &pidWPS, &tidWPS))
                 && (hwndNotify)
               )
            {
                // yes:
                PXWPFILEDLG pfdShared = NULL;

                // check how much memory we need for WinFileDlg

                // 1) at least the size of FILEDLG
                ULONG       cbShared = sizeof(XWPFILEDLG);

                // 2) add memory for extra fields
                if (pfd->pszTitle)
                    cbShared += strlen(pfd->pszTitle) + 1;
                if (pfd->pszOKButton)
                    cbShared += strlen(pfd->pszOKButton) + 1;

                // 3) types array
                if (pfd->pszIType)
                    cbShared += strlen(pfd->pszIType) + 1;
                if (pfd->papszITypeList)
                {
                    PSZ *ppszThis = pfd->papszITypeList[0];
                    while (*ppszThis)
                    {
                        cbShared += strlen(*ppszThis) + 1;
                        ppszThis++;
                    }
                }

                // 4) drives array
                if (pfd->pszIType)
                    cbShared += strlen(pfd->pszIDrive) + 1;
                if (pfd->papszIDriveList)
                {
                    PSZ *ppszThis = pfd->papszIDriveList[0];
                    while (*ppszThis)
                    {
                        cbShared += strlen(*ppszThis) + 1;
                        ppszThis++;
                    }
                }

                // OK, now we know how much memory we need...
                // allocate a block of shared memory with this size
                arc = DosAllocSharedMem((PVOID*)&pfdShared,
                                        NULL,       // unnamed
                                        cbShared,
                                        PAG_COMMIT | OBJ_GIVEABLE | OBJ_TILE
                                            | PAG_READ | PAG_WRITE);
                if (    (arc == NO_ERROR)
                     && (pfdShared)
                   )
                {
                    // OK, we got shared memory:
                    ULONG   cbThis;
                    PBYTE   pb = (PBYTE)pfdShared + sizeof(XWPFILEDLG);
                                // current offset where to copy to

                    // clear it
                    memset(pfdShared, 0, cbShared);

                    // copy owner window
                    pfdShared->hwndOwner = hwndO;

                    // copy FILEDLG
                    memcpy(&pfdShared->fd,
                           pfd,
                           min(pfd->cbSize, sizeof(FILEDLG)));

                    // now copy the various fields AFTER the XWPFILEDLG
                    if (pfd->pszTitle)
                    {
                        cbThis = strlen(pfd->pszTitle) + 1;
                        memcpy(pb, pfd->pszTitle, cbThis);
                        pb += cbThis;
                    }
                    if (pfd->pszOKButton)
                    {
                        cbThis = strlen(pfd->pszOKButton) + 1;
                        memcpy(pb, pfd->pszTitle, cbThis);
                        pb += cbThis;
                    }

                    // types array
                    if (pfd->pszIType)
                    {
                        cbThis = strlen(pfd->pszIType) + 1;
                        memcpy(pb, pfd->pszTitle, cbThis);
                        pb += cbThis;
                    }
                    if (pfd->papszITypeList)
                    {
                        PSZ *ppszThis = pfd->papszITypeList[0];
                        while (*ppszThis)
                        {
                            cbThis = strlen(*ppszThis) + 1;
                            memcpy(pb, pfd->pszTitle, cbThis);
                            pb += cbThis;

                            ppszThis++;
                        }
                    }

                    // drives array
                    if (pfd->pszIDrive)
                    {
                        cbThis = strlen(pfd->pszIDrive) + 1;
                        memcpy(pb, pfd->pszTitle, cbThis);
                        pb += cbThis;
                    }
                    if (pfd->papszIDriveList)
                    {
                        PSZ *ppszThis = pfd->papszIDriveList[0];
                        while (*ppszThis)
                        {
                            cbThis = strlen(*ppszThis) + 1;
                            memcpy(pb, pfd->pszTitle, cbThis);
                            pb += cbThis;

                            ppszThis++;
                        }
                    }

                    // OK, now we got everything in shared memory...
                    // give the WPS access to it
                    arc = DosGiveSharedMem(pfdShared,
                                           pidWPS,
                                           PAG_READ | PAG_WRITE);
                    if (arc == NO_ERROR)
                    {
                        // send this block to XFLDR.DLL;
                        // we can't use WinSendMsg because this would
                        // block the msg queue for the calling thread.
                        // So instead we use WinPostMsg and wait for
                        // XFLDR.DLL to post something back to our
                        // temporary object window here...

                        pfdShared->hwndNotify = hwndNotify;

                        if (WinPostMsg(pXwpGlobalShared->hwndAPIObject,
                                       APIM_FILEDLG,
                                       (MPARAM)pfdShared,
                                       0))
                        {
                            // OK, the file dialog is showing now...
                            // keep processing our message queue until
                            // XFLDR.DLL posts back WM_USER to the
                            // object window
                            QMSG    qmsg;
                            BOOL    fQuit = FALSE;
                            while (    (WinGetMsg(hab,
                                                  &qmsg, 0, 0, 0))
                                    && (!fQuit)
                                  )
                            {
                                // current message for our object window?
                                if (    (qmsg.hwnd == hwndNotify)
                                     && (qmsg.msg == WM_USER)
                                   )
                                    // yes: this means the file dlg has been
                                    // dismissed, and we can get outta here
                                    fQuit = TRUE;

                                WinDispatchMsg(hab, &qmsg);
                            }

                            hwndReturn = pfdShared->hwndReturn;

                            // copy stuff back into regular FILEDLG (WinFileDlg)
                            memcpy(pfd,
                                   &pfdShared->fd,
                                   min(pfd->cbSize, sizeof(FILEDLG)));
                        }
                    } // end DosGiveSharedMem(pfdShared,

                    DosFreeMem(pfdShared);

                } // end DosAllocSharedMem

            } // end if hwndNotify and stuff

            WinDestroyWindow(hwndNotify);
        }
    }

    return (hwndReturn);
}

/*
 *@@ SuperFileDlg:
 *
 */

BOOL SuperFileDlg(HWND hwndOwner,    // in: owner for file dlg
                  PSZ pszFile,       // in: file mask; out: fully q'd filename
                                     //    (should be CCHMAXPATH in size)
                  ULONG flFlags,     // in: any combination of the following:
                                     // -- WINH_FOD_SAVEDLG: save dlg; else open dlg
                                     // -- WINH_FOD_INILOADDIR: load FOD path from INI
                                     // -- WINH_FOD_INISAVEDIR: store FOD path to INI on OK
                  HINI hini,         // in: INI file to load/store last path from (can be HINI_USER)
                  const char *pcszApplication, // in: INI application to load/store last path from
                  const char *pcszKey)        // in: INI key to load/store last path from
{
    FILEDLG fd;
    memset(&fd, 0, sizeof(FILEDLG));
    fd.cbSize = sizeof(FILEDLG);
    fd.fl = FDS_CENTER;

    if (flFlags & WINH_FOD_SAVEDLG)
        fd.fl |= FDS_SAVEAS_DIALOG;
    else
        fd.fl |= FDS_OPEN_DIALOG;

    // default: copy pszFile
    strcpy(fd.szFullFile, pszFile);

    if ( (hini) && (flFlags & WINH_FOD_INILOADDIR) )
    {
        // overwrite with initial directory for FOD from OS2.INI
        if (PrfQueryProfileString(hini,
                                  pcszApplication,
                                  pcszKey,
                                  "",      // default string V0.9.9 (2001-02-10) [umoeller]
                                  fd.szFullFile,
                                  sizeof(fd.szFullFile)-10)
                    >= 2)
        {
            // found: append "\*"
            strcat(fd.szFullFile, "\\");
            strcat(fd.szFullFile, pszFile);
        }
    }

    if (    WinFileDlg(HWND_DESKTOP,    // parent
                       hwndOwner, // owner
                       &fd)
        && (fd.lReturn == DID_OK)
       )
    {
        // save path back?
        if (    (hini)
             && (flFlags & WINH_FOD_INISAVEDIR)
           )
        {
            // get the directory that was used
            PSZ p = strrchr(fd.szFullFile, '\\');
            if (p)
            {
                // contains directory:
                // copy to OS2.INI
                PSZ pszDir = strhSubstr(fd.szFullFile, p);
                if (pszDir)
                {
                    PrfWriteProfileString(hini,
                                          pcszApplication,
                                          pcszKey,
                                          pszDir);
                    free(pszDir);
                }
            }
        }

        strcpy(pszFile, fd.szFullFile);

        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ LoadXMLFile:
 *      loads and parses an XML file into an XMLFILE
 *      structure, which is created.
 *
 *      If this returns ERROR_DOM_PARSING or
 *      ERROR_DOM_VALIDITY, ppszError
 *      receives an error description, which the
 *      caller must free.
 *
 *@@added V0.9.9 (2001-02-10) [umoeller]
 */

APIRET LoadXMLFile(const char *pcszFilename,    // in: file:/K:\...
                   PXMLFILE *ppXMLFile,         // out: loaded file
                   PSZ *ppszError)     // out: error string
{
    APIRET  arc = NO_ERROR;

    PSZ     pszContents = NULL;

    arc = doshLoadTextFile(pcszFilename, &pszContents);

    if (arc == NO_ERROR)
    {
        PXMLFILE pFile = (PXMLFILE)malloc(sizeof(XMLFILE));
        if (!pFile)
            arc = ERROR_NOT_ENOUGH_MEMORY;
        else
        {
            memset(pFile, 0, sizeof(*pFile));

            xstrInitSet(&pFile->strContents, pszContents);

            xstrConvertLineFormat(&pFile->strContents,
                                  CRLF2LF);

            arc = xmlCreateDOM(DF_PARSEDTD,
                               &pFile->pDom);
            if (arc == NO_ERROR)
            {
                arc = xmlParse(pFile->pDom,
                               pFile->strContents.psz,
                               pFile->strContents.ulLength,
                               TRUE);       // last

                if (arc == ERROR_DOM_PARSING)
                {
                    CHAR    sz[1000];
                    sprintf(sz,
                            "Parsing error: %s (line %d, column %d)",
                            pFile->pDom->pcszErrorDescription,
                            pFile->pDom->ulErrorLine,
                            pFile->pDom->ulErrorColumn);

                    if (pFile->pDom->pxstrFailingNode)
                        sprintf(sz + strlen(sz),
                                " (%s)",
                                pFile->pDom->pxstrFailingNode->psz);

                    *ppszError = strdup(sz);
                }
                else if (arc == ERROR_DOM_VALIDITY)
                {
                    CHAR    sz[1000];
                    sprintf(sz,
                            "Validation error: %s (line %d, column %d)",
                            pFile->pDom->pcszErrorDescription,
                            pFile->pDom->ulErrorLine,
                            pFile->pDom->ulErrorColumn);

                    if (pFile->pDom->pxstrFailingNode)
                        sprintf(sz + strlen(sz),
                                " (%s)",
                                pFile->pDom->pxstrFailingNode->psz);

                    *ppszError = strdup(sz);
                }
            }

            if (arc == NO_ERROR)
                *ppXMLFile = pFile;
            else
                // error:
                free(pFile);
        }
    }

    return (arc);
}

/*
 *@@ FreeXMLFile:
 *
 */

VOID FreeXMLFile(PXMLFILE pFile)
{
    xmlFreeDOM(pFile->pDom);
}

/* ******************************************************************
 *
 *   XML view
 *
 ********************************************************************/

/*
 *@@ INSERTSTACK:
 *
 */

typedef struct _INSERTSTACK
{
    PNODERECORD     pParentRecord;
    PXMLDOM         pDom;
} INSERTSTACK, *PINSERTSTACK;

/*
 *@@ InsertAttribDecls:
 *
 *@@added V0.9.9 (2001-02-16) [umoeller]
 */

VOID XWPENTRY InsertAttribDecls(TREE *t,        // in: PCMATTRIBUTEDECL really
                                PVOID pUser)    // in: PINSERTSTACK user param
{
    PCMATTRIBUTEDECL pAttribDecl = (PCMATTRIBUTEDECL)t;
    PINSERTSTACK pInsertStack = (PINSERTSTACK)pUser;

    PNODERECORD prec = (PNODERECORD)cnrhAllocRecords(G_hwndCnr,
                                                     sizeof(NODERECORD),
                                                     1);

    XSTRING str;
    xstrInitCopy(&str, "attrib: \"", 0);
    xstrcats(&str, &pAttribDecl->NodeBase.strNodeName);
    xstrcat(&str, "\", type: ", 0);
    switch (pAttribDecl->ulAttrType)
    {
        case CMAT_ENUM:
        {
            xstrcat(&str, "ENUM(", 0);
            PNODEBASE pNode = (PNODEBASE)treeFirst(pAttribDecl->ValuesTree);
            while (pNode)
            {
                xstrcats(&str, &pNode->strNodeName);
                xstrcatc(&str, '|');

                pNode = (PNODEBASE)treeNext(&pNode->Tree);
            }
            /* treeTraverse(pAttribDecl->ValuesTree,
                         CatAttribEnums,
                         &str,
                         0); */
            *(str.psz + str.ulLength - 1) = ')';
        break; }

        case CMAT_CDATA:
            xstrcat(&str, "CDATA", 0);
        break;
        case CMAT_ID:
            xstrcat(&str, "ID", 0);
        break;
        case CMAT_IDREF:
            xstrcat(&str, "IDREF", 0);
        break;
        case CMAT_IDREFS:
            xstrcat(&str, "IDREFS", 0);
        break;
        case CMAT_ENTITY:
            xstrcat(&str, "ENTITY", 0);
        break;
        case CMAT_ENTITIES:
            xstrcat(&str, "ENTITIES", 0);
        break;
        case CMAT_NMTOKEN:
            xstrcat(&str, "NMTOKEN", 0);
        break;
        case CMAT_NMTOKENS:
            xstrcat(&str, "NMTOKENS", 0);
        break;
    }

    cnrhInsertRecords(G_hwndCnr,
                      (PRECORDCORE)pInsertStack->pParentRecord,
                      (PRECORDCORE)prec,
                      TRUE,
                      str.psz,
                      CRA_RECORDREADONLY,
                      1);
}

/*
 *@@ InsertEDParticle:
 *
 *@@added V0.9.9 (2001-02-16) [umoeller]
 */

PNODERECORD InsertEDParticle(PCMELEMENTPARTICLE pParticle,
                             PNODERECORD pParentRecord)
{
    PNODERECORD prec = (PNODERECORD)cnrhAllocRecords(G_hwndCnr,
                                                     sizeof(NODERECORD),
                                                     1);

    XSTRING str;
    xstrInitCopy(&str, "\"", 0);
    xstrcats(&str, &pParticle->NodeBase.strNodeName);

    switch (pParticle->ulRepeater)
    {
        // XML_CQUANT_NONE

        case XML_CQUANT_OPT:
            xstrcatc(&str, '?');
        break;

        case XML_CQUANT_REP:
            xstrcatc(&str, '*');
        break;

        case XML_CQUANT_PLUS:
            xstrcatc(&str, '+');
        break;
    }

    const char *pcsz = "unknown";
    switch (pParticle->NodeBase.ulNodeType)
    {
        case ELEMENTPARTICLE_EMPTY:
            pcsz = "\" EMPTY";
        break;

        case ELEMENTPARTICLE_ANY:
            pcsz = "\" ANY";
        break;

        case ELEMENTPARTICLE_MIXED:
            pcsz = "\" MIXED";
        break;

        case ELEMENTPARTICLE_CHOICE:
            pcsz = "\" CHOICE";
        break;

        case ELEMENTPARTICLE_SEQ:
            pcsz = "\" SEQ";
        break;

        case ELEMENTPARTICLE_NAME:        // subtypes
            pcsz = "\" NAME";
        break;
    }

    xstrcat(&str, pcsz, 0);

    cnrhInsertRecords(G_hwndCnr,
                      (PRECORDCORE)pParentRecord,
                      (PRECORDCORE)prec,
                      TRUE,
                      str.psz,
                      CRA_RECORDREADONLY,
                      1);

    if (pParticle->pllSubNodes)
    {
        PLISTNODE pNode = lstQueryFirstNode(pParticle->pllSubNodes);
        while (pNode)
        {
            PCMELEMENTPARTICLE pSub = (PCMELEMENTPARTICLE)pNode->pItemData;
            InsertEDParticle(pSub,
                             prec);

            pNode = pNode->pNext;
        }
    }

    return (prec);
}

/*
 *@@ InsertElementDecls:
 *      tree traversal function initiated from InsertDocType.
 *
 *@@added V0.9.9 (2001-02-16) [umoeller]
 */

VOID XWPENTRY InsertElementDecls(TREE *t,           // in: an CMELEMENTDECLNODE really
                                 PVOID pUser)       // in: PINSERTSTACK user param
{
    PCMELEMENTDECLNODE pElementDecl = (PCMELEMENTDECLNODE)t;
    PINSERTSTACK pInsertStack = (PINSERTSTACK)pUser;

    // insert the element declaration root particle and subparticles
    // (this recurses)
    PNODERECORD pElementRec = InsertEDParticle(&pElementDecl->Particle,
                                               pInsertStack->pParentRecord);

    // now insert attribute decls, if any
    PCMATTRIBUTEDECLBASE pAttribDeclBase
        = xmlFindAttribDeclBase(pInsertStack->pDom,
                                &pElementDecl->Particle.NodeBase.strNodeName);
    if (pAttribDeclBase)
    {
        // traverse the attributes tree as well
        INSERTSTACK Stack2 = {pElementRec, pInsertStack->pDom};

        treeTraverse(pAttribDeclBase->AttribDeclsTree,
                     InsertAttribDecls,
                     &Stack2,       // user param
                     0);
    }
}

/*
 *@@ InsertDocType:
 *
 *@@added V0.9.9 (2001-02-16) [umoeller]
 */

VOID InsertDocType(PXMLDOM pDom,
                   PDOMDOCTYPENODE pDocTypeNode,
                   PNODERECORD pParentRecord)
{
    INSERTSTACK InsertStack = {pParentRecord, pDom};

    treeTraverse(pDocTypeNode->ElementDeclsTree,
                 InsertElementDecls,
                 &InsertStack,         // user param
                 0);
}

VOID InsertDom(PXMLDOM pDom,
               PDOMNODE pDomNode,
               PNODERECORD pParentRecord);

/*
 *@@ InsertDom:
 *
 *      Note: We get the document node first, which we must
 *      not insert, but only its children.
 */

VOID InsertDom(PXMLDOM pDom,
               PDOMNODE pDomNode,
               PNODERECORD pParentRecord)       // initially NULL
{
    BOOL        fInsertChildren = FALSE;
    PNODERECORD prec = NULL;
    PDOMDOCTYPENODE pDocType = NULL;

    XSTRING     str;
    xstrInit(&str, 100);

    switch (pDomNode->NodeBase.ulNodeType)
    {
        case DOMNODE_ELEMENT:
            xstrcpys(&str, &pDomNode->NodeBase.strNodeName);
        break;

        case DOMNODE_ATTRIBUTE:
            xstrcpys(&str, &pDomNode->NodeBase.strNodeName);
            xstrcat(&str, "=\"", 2);
            xstrcats(&str, pDomNode->pstrNodeValue);
            xstrcatc(&str, '\"');
        break;

        case DOMNODE_TEXT:
            xstrcpy(&str, "\"", 1);
            xstrcats(&str, pDomNode->pstrNodeValue);
            xstrcatc(&str, '\"');
        break;

        case DOMNODE_DOCUMENT:
            xstrcpy(&str, "Document", 0);
            xstrcat(&str, " \"", 0);
            xstrcats(&str, &pDomNode->NodeBase.strNodeName);
            xstrcat(&str, "\"", 0);
        break;

        case DOMNODE_DOCUMENT_TYPE:
        {
            pDocType = (PDOMDOCTYPENODE)pDomNode;
            xstrcpy(&str, "DOCTYPE system: \"", 0);
            xstrcats(&str, &pDocType->strSystemID);
            xstrcat(&str, "\", public: \"", 0);
            xstrcats(&str, &pDocType->strPublicID);
            xstrcatc(&str, '\"');
        break; }

        default:
        {
            CHAR sz[1000];
            sprintf(sz, "Unknown node type %d", pDomNode->NodeBase.ulNodeType);
            xstrcpy(&str, sz, 0);

        break; }
    }

    if (str.ulLength)
    {
        prec = (PNODERECORD)cnrhAllocRecords(G_hwndCnr,
                                             sizeof(NODERECORD),
                                             1);

        cnrhInsertRecords(G_hwndCnr,
                          (PRECORDCORE)pParentRecord,
                          (PRECORDCORE)prec,
                          TRUE,
                          str.psz,
                          CRA_RECORDREADONLY,
                          1);
        fInsertChildren = TRUE;
    }

    if (fInsertChildren)
    {
        // insert attributes
        INSERTSTACK Stack2 = {prec, pDom};
        PDOMNODE pAttrNode = (PDOMNODE)treeFirst(pDomNode->AttributesMap);
        while (pAttrNode)
        {
            InsertDom(pDom,
                      pAttrNode,
                      prec);
            pAttrNode = (PDOMNODE)treeNext((TREE*)pAttrNode);
        }

        // and children
        PLISTNODE pSubNode = lstQueryFirstNode(&pDomNode->llChildren);
        while (pSubNode)
        {
            PDOMNODE pDomSubnode = (PDOMNODE)pSubNode->pItemData;
            InsertDom(pDom,
                      pDomSubnode,
                      prec);

            pSubNode = pSubNode->pNext;
        }
    }
    else
        xstrClear(&str);

    if (prec && pDocType)
        InsertDocType(pDom, pDocType, prec);
}

/* ******************************************************************
 *
 *   Container window proc
 *
 ********************************************************************/

/*
 *@@ LoadAndInsert:
 *
 */

VOID LoadAndInsert(const char *pcszFile)
{
    PSZ pszError;
    APIRET  arc = LoadXMLFile(pcszFile,
                              &G_pLoadedFile,
                              &pszError);
    if (    (arc == ERROR_DOM_PARSING)
         || (arc == ERROR_DOM_VALIDITY)
       )
    {
        winhSetWindowText(G_hwndStatusBar,
                          pszError);
        free(pszError);
    }
    else if (arc != NO_ERROR)
    {
        winhSetWindowText(G_hwndStatusBar,
                          "Error %d loading \"%s\".",
                          arc,
                          pcszFile);
    }
    else
    {
        winhSetWindowText(G_hwndStatusBar,
                          "File \"%s\" loaded, %d bytes",
                          pcszFile,
                          G_pLoadedFile->strContents.ulLength);

        cnrhRemoveAll(G_hwndCnr);

        InsertDom(G_pLoadedFile->pDom,
                  (PDOMNODE)G_pLoadedFile->pDom->pDocumentNode,
                  NULL);
    }
}

/*
 *@@ fnwpSubclassedCnr:
 *
 */

MRESULT EXPENTRY fnwpSubclassedCnr(HWND hwndCnr, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CLOSE:
            winhSaveWindowPos(G_hwndMain,
                              HINI_USER,
                              INIAPP,
                              INIKEY_MAINWINPOS);
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
        break;

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                case IDMI_OPEN:
                {
                    CHAR    szFile[CCHMAXPATH] = "*.xml";
                    if (SuperFileDlg(G_hwndMain,
                                     szFile,
                                     WINH_FOD_INILOADDIR | WINH_FOD_INISAVEDIR,
                                     HINI_USER,
                                     INIAPP,
                                     INIKEY_OPENDLG))
                        LoadAndInsert(szFile);
                break; }

                case IDMI_EXIT:
                    WinPostMsg(G_hwndMain,
                               WM_SYSCOMMAND,
                               (MPARAM)SC_CLOSE,
                               0);
                break;
            }
        break;

        default:
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ SetupCnr:
 *
 */

VOID SetupCnr(HWND hwndCnr)
{
    BEGIN_CNRINFO()
    {
        // switch view
        cnrhSetView(CV_TREE | CA_TREELINE | CV_TEXT);
        cnrhSetTreeIndent(30);
    } END_CNRINFO(hwndCnr)

    winhSetWindowFont(hwndCnr, NULL);
}

/* ******************************************************************
 *
 *   Frame window proc
 *
 ********************************************************************/

/*
 *@@ winhCreateStatusBar:
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

HWND winhCreateStatusBar(HWND hwndFrame,
                         HWND hwndOwner,
                         USHORT usID,
                         PSZ pszFont,
                         LONG lColor)
{
    // create status bar
    HWND        hwndReturn = NULLHANDLE;
    PPRESPARAMS ppp = NULL;
    winhStorePresParam(&ppp, PP_FONTNAMESIZE, strlen(pszFont)+1, pszFont);
    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);
    winhStorePresParam(&ppp, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = CLR_BLACK;
    winhStorePresParam(&ppp, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
    hwndReturn = WinCreateWindow(G_hwndMain,
                                  WC_STATIC,
                                  "Welcome.",
                                  SS_TEXT | DT_VCENTER | WS_VISIBLE,
                                  0, 0, 0, 0,
                                  hwndOwner,
                                  HWND_TOP,
                                  usID,
                                  NULL,
                                  ppp);
    free(ppp);
    return (hwndReturn);
}

static PRECORDCORE s_preccExpanded = NULL;

/*
 *@@ winh_fnwpFrameWithStatusBar:
 *      subclassed frame window proc.
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

MRESULT EXPENTRY winh_fnwpFrameWithStatusBar(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_QUERYFRAMECTLCOUNT:
        {
            // query the standard frame controls count
            ULONG ulrc = (ULONG)(G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2));

            // if we have a status bar, increment the count
            ulrc++;

            mrc = (MPARAM)ulrc;
        break; }

        case WM_FORMATFRAME:
        {
            //  query the number of standard frame controls
            ULONG ulCount = (ULONG)(G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2));

            // we have a status bar:
            // format the frame
            ULONG       ul;
            PSWP        swpArr = (PSWP)mp1;

            for (ul = 0; ul < ulCount; ul++)
            {
                if (WinQueryWindowUShort( swpArr[ul].hwnd, QWS_ID ) == 0x8008 )
                                                                 // FID_CLIENT
                {
                    POINTL      ptlBorderSizes;
                    ULONG       ulStatusBarHeight = 20;
                    WinSendMsg(hwndFrame,
                               WM_QUERYBORDERSIZE,
                               (MPARAM)&ptlBorderSizes,
                               0);

                    // first initialize the _new_ SWP for the status bar.
                    // Since the SWP array for the std frame controls is
                    // zero-based, and the standard frame controls occupy
                    // indices 0 thru ulCount-1 (where ulCount is the total
                    // count), we use ulCount for our static text control.
                    swpArr[ulCount].fl = SWP_MOVE | SWP_SIZE | SWP_NOADJUST | SWP_ZORDER;
                    swpArr[ulCount].x  = ptlBorderSizes.x;
                    swpArr[ulCount].y  = ptlBorderSizes.y;
                    swpArr[ulCount].cx = swpArr[ul].cx;  // same as cnr's width
                    swpArr[ulCount].cy = ulStatusBarHeight;
                    swpArr[ulCount].hwndInsertBehind = HWND_BOTTOM; // HWND_TOP;
                    swpArr[ulCount].hwnd = G_hwndStatusBar;

                    // adjust the origin and height of the container to
                    // accomodate our static text control
                    swpArr[ul].y  += swpArr[ulCount].cy;
                    swpArr[ul].cy -= swpArr[ulCount].cy;
                }
            }

            // increment the number of frame controls
            // to include our status bar
            mrc = (MRESULT)(ulCount + 1);
        break; }

        case WM_CALCFRAMERECT:
        {
            mrc = G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2);

            // we have a status bar: calculate its rectangle
            // CalcFrameRect(mp1, mp2);
        break; }

        case WM_CONTROL:
            if (SHORT1FROMMP(mp1) == FID_CLIENT)
            {
                switch (SHORT2FROMMP(mp1))
                {
                    case CN_EXPANDTREE:
                        s_preccExpanded = (PRECORDCORE)mp2;
                        WinStartTimer(WinQueryAnchorBlock(hwndFrame),
                                      hwndFrame,
                                      1,
                                      100);
                    break;
                }
            }
        break;

        case WM_TIMER:
            // stop timer (it's just for one shot)
            WinStopTimer(WinQueryAnchorBlock(hwndFrame),
                         hwndFrame,
                         (ULONG)mp1);       // timer ID

            switch ((ULONG)mp1)        // timer ID
            {
                case 1:
                {
                    if (s_preccExpanded->flRecordAttr & CRA_EXPANDED)
                    {
                        HWND hwndCnr = WinWindowFromID(hwndFrame, FID_CLIENT);
                        PRECORDCORE     preccLastChild;
                        // scroll the tree view properly
                        preccLastChild = (PRECORDCORE)WinSendMsg(hwndCnr,
                                                    CM_QUERYRECORD,
                                                    s_preccExpanded,
                                                            // expanded PRECORDCORE from CN_EXPANDTREE
                                                    MPFROM2SHORT(CMA_LASTCHILD,
                                                                 CMA_ITEMORDER));
                        if (preccLastChild)
                        {
                            // ULONG ulrc;
                            cnrhScrollToRecord(hwndCnr,
                                    (PRECORDCORE)preccLastChild,
                                    CMA_TEXT,   // record text rectangle only
                                    TRUE);      // keep parent visible
                        }
                    }
                break; }

            }
        break;

        default:
            mrc = G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   main
 *
 ********************************************************************/

#define ARRAYITEMCOUNT(array) sizeof(array) / sizeof(array[0])

/* typedef struct _CONTROLDEF
{
    const char  *pcszClass;         // registered PM window class
    const char  *pcszText;          // window text (class-specific);
                                    // NULL for tables

    ULONG       flStyle;

    ULONG       ulID;

    USHORT      usAdjustPosition;
            // flags for winhAdjustControls; any combination of
            // XAC_MOVEX, XAC_MOVEY, XAC_SIZEX, XAC_SIZEY

    SIZEL       szlControl;         // proposed size
    ULONG       ulSpacing;          // spacing around control

} CONTROLDEF, *PCONTROLDEF; */

static CONTROLDEF
            Static1 =
                    {
                        WC_STATIC,
                        "Test text row 1 column 1",
                        WS_VISIBLE | SS_TEXT | DT_LEFT | DT_TOP,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { 300, 30 },
                        5
                    },
            Static2 =
                    {
                        WC_STATIC,
                        "Test text row 1 column 2",
                        WS_VISIBLE | SS_TEXT | DT_LEFT | DT_TOP,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { 300, 30 },
                        5
                    },
            Cnr =
                    {
                        WC_CONTAINER,
                        NULL,
                        WS_VISIBLE | WS_TABSTOP,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { 100, 100 },
                        5
                    },
            CnrGroup =
                    {
                        WC_STATIC,
                        "Container1",
                        WS_VISIBLE | SS_GROUPBOX | DT_MNEMONIC,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { 100, 100 },       // ignored
                        5
                    },
            OKButton =
                    {
                        WC_BUTTON,
                        "~OK",
                        WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_DEFAULT,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { SZL_AUTOSIZE, 30 },
                        5
                    },
            CancelButton =
                    {
                        WC_BUTTON,
                        "~Cancel",
                        WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        0,
                        CTL_COMMON_FONT,
                        0,
                        { SZL_AUTOSIZE, 30 },
                        5
                    };

static _DLGHITEM aDlgItems[] =
                {
                    START_TABLE,
                        START_ROW(0),
                            CONTROL_DEF(&Static1),
                        START_ROW(0),
                            START_GROUP_TABLE(&CnrGroup),
                                START_ROW(0),
                                    CONTROL_DEF(&Cnr),
                            END_TABLE,
                            START_GROUP_TABLE(&CnrGroup),
                                START_ROW(0),
                                    CONTROL_DEF(&Cnr),
                                START_ROW(0),
                                    CONTROL_DEF(&OKButton),
                                START_ROW(0),
                                    CONTROL_DEF(&CancelButton),
                            END_TABLE,
                        START_ROW(0),
                            CONTROL_DEF(&Static2),

                        START_ROW(0),
                            CONTROL_DEF(&OKButton),
                            CONTROL_DEF(&CancelButton),
                    END_TABLE
                };

/*
 *@@ main:
 *
 */

int main(int argc, char* argv[])
{
    HAB         hab;
    HMQ         hmq;
    QMSG        qmsg;

    if (!(hab = WinInitialize(0)))
        return FALSE;

    if (!(hmq = WinCreateMsgQueue(hab, 0)))
        return FALSE;

    DosError(FERR_DISABLEHARDERR | FERR_ENABLEEXCEPTION);

    HWND hwndDlg = NULLHANDLE;
    APIRET arc = dlghCreateDlg(&hwndDlg,
                               NULLHANDLE,      // owner
                               FCF_TITLEBAR | FCF_SYSMENU | FCF_DLGBORDER | FCF_NOBYTEALIGN,
                               WinDefDlgProc,
                               "DemoDlg",
                               aDlgItems,
                               ARRAYITEMCOUNT(aDlgItems),
                               NULL,
                               "9.WarpSans");
    if (arc == NO_ERROR)
    {
        WinProcessDlg(hwndDlg);
        WinDestroyWindow(hwndDlg);
    }
    else
    {
        CHAR szErr[100];
        sprintf(szErr, "Error %d", arc);
        winhDebugBox(NULLHANDLE, "Error", szErr);
    }

    /*
    // create frame and container
    G_hwndMain = winhCreateStdWindow(HWND_DESKTOP,
                                     0,
                                     FCF_TITLEBAR
                                        | FCF_SYSMENU
                                        | FCF_MINMAX
                                        | FCF_SIZEBORDER
                                        | FCF_ICON
                                        | FCF_MENU
                                        | FCF_TASKLIST,
                                     0,
                                     APPTITLE,      // xmlview
                                     1,     // icon resource
                                     WC_CONTAINER,
                                     CCS_MINIICONS | CCS_READONLY | CCS_EXTENDSEL
                                        | WS_VISIBLE,
                                     0,
                                     NULL,
                                     &G_hwndCnr);

    if ((G_hwndMain) && (G_hwndCnr))
    {
        // subclass cnr (it's our client)
        G_pfnwpCnrOrig = WinSubclassWindow(G_hwndCnr, fnwpSubclassedCnr);

        // create status bar as child of the frame
        G_hwndStatusBar = winhCreateStatusBar(G_hwndMain,
                                              G_hwndCnr,
                                              0,
                                              "9.WarpSans",
                                              CLR_BLACK);
        // subclass frame for supporting status bar and msgs
        G_fnwpFrameOrig = WinSubclassWindow(G_hwndMain,
                                            winh_fnwpFrameWithStatusBar);

        SetupCnr(G_hwndCnr);

        if (!winhRestoreWindowPos(G_hwndMain,
                                  HINI_USER,
                                  INIAPP,
                                  INIKEY_MAINWINPOS,
                                  SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE))
            WinSetWindowPos(G_hwndMain,
                            HWND_TOP,
                            10, 10, 500, 500,
                            SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE);

        if (argc > 1)
            LoadAndInsert(argv[1]);

        //  standard PM message loop
        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
        {
            WinDispatchMsg(hab, &qmsg);
        }
    } */

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    return (0);
}

