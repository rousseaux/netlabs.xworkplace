
/*
 *@@sourcefile strat_init_base.c:
 *      PDD "init_base" strategy routine.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>
#include <secure.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"
#include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

#include "bldlevel.h"

/* ******************************************************************
 *                                                                  *
 *   Prototypes                                                     *
 *                                                                  *
 ********************************************************************/

extern int _System sec32_pre_init_base(PTR16 reqpkt);

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

extern unsigned short codeend;      // in sec32_start.asm
extern unsigned short dataend;      // in sec32_start.asm
extern PTR16          DevHelp2;

extern char code32_end;
extern char data32_end;

extern void begin_code32(void);     // in sec32_start.asm
extern int  begin_data32;           // in sec32_start.asm

char banner[] = "XWPSEC32.SYS: XWorkplace Security driver V" BLDLEVEL_VERSION " (" __DATE__ ") loaded\r\n"
                "(C) Copyright 2000 Ulrich M”ller\r\n";

#pragma pack(1)

/*
 *@@ ddd32_parm_list:
 *      a pointer to this struct is in the INIT
 *      request packet. This allows us to retrieve
 *      the driver command line and such.
 */

struct ddd32_parm_list
{
    unsigned short cache_parm_list;        // addr of InitCache_Parameter List
    unsigned short disk_config_table;      // addr of disk_configuration table
    unsigned short init_req_vec_table;     // addr of IRQ_Vector_Table
    unsigned short cmd_line_args;          // addr of Command Line Args
    unsigned short machine_config_table;   // addr of Machine Config Info
};
#pragma pack()


void *TKSSBase;
    // pointer to kernel _TKSSBase value.
PTR16          DevHelp2;

// struct DevHelp32 DevHelp32 = {DEVHELP32_MAGIC, DEVHELP32_VERSION, };
    // mwdd32.sys DevHlp32 entry points

struct SecExp_s SecurityExports = {SEC_EXPORT_MAJOR_VERSION , SEC_EXPORT_MINOR_VERSION , };
    // SecHlp kernel entry points

struct SecurityHelpers SecHlp = {0, };
    // SecHlp sesdd32.sys entry points

/* ******************************************************************
 *                                                                  *
 *   Helper functions                                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ FixMemory:
 *      called by sec32_init_base() to lock our data
 *      and code segments into physical memory.
 *
 *      Returns NO_ERROR or error code.
 */

int FixMemory(void)
{
    APIRET rc;
    ULONG  PgCount;
    char   lock[12];            // receives lock handle

    // lock DGROUP into physical memory
    rc = DevHlp32_VMLock(VMDHL_LONG          // lock for more than 2 seconds
                            | VMDHL_WRITE,    // write (set "dirty bit" in page table entries)
                            // | VMDHL_VERIFY,
                            // since VMDHL_NOBLOCK is not specified, this blocks
                            // until the page is available
                         &begin_data32,
                         (ULONG)(&data32_end) - (ULONG)(&begin_data32),
                         (void *)-1,
                         __StackToFlat(lock),        // receives lock handle
                         __StackToFlat(&PgCount));

    if (rc == NO_ERROR)
    {
        // locks CODE32 into physical memory
        rc = DevHlp32_VMLock(VMDHL_LONG,
                                 // | VMDHL_VERIFY,
                             (VOID*)&begin_code32,
                             // (void*)begin_code32,
                             // (ULONG)(&code32_end) - (ULONG)begin_code32,
                             (ULONG)(&code32_end) - (ULONG)(&begin_code32),
                             (void *)-1,
                             __StackToFlat(lock),
                             __StackToFlat(&PgCount));
        /* if (rc == NO_ERROR)
        {
            // allocate 2 GDT selectors to communicate with the control program
            // later; we must do this at init time, but right now these
            // selectors point to nowhere until the CP calls the REGISTER IOCtl
            rc = DevHlp32_AllocGDTSelector(G_aGDTSels, 2);
            if (rc == NO_ERROR)
            {
                // set the pointers to the control program communication buffer;
                G_CPData.pOpData = NULL; // (OPDATA*)MK_FP(GDTSels[0], 0);
                G_CPData.pvBuf = NULL; // (void*)MK_FP(GDTSels[1], 0);

                // say the control program has never been attached
                G_fCPAttached = -1;
            }
        } */
    }

    return rc;
}

/*
 *@@ InitSecurity:
 *      interfaces SES to pass our kernel hooks and
 *      get the security helpes.
 */

int InitSecurity(VOID)
{
    APIRET rc = NO_ERROR;

    // pass table of security hooks to kernel
    if ((rc = DevHlp32_Security(DHSEC_SETIMPORT,
                                &SecurityImports)) // in sec32_callbacks.c
                == NO_ERROR)
    {
        // retrieve security helpers
        if ((rc = sec32_attach_ses((void *)&SecHlp)) == NO_ERROR)
        {
            rc = DevHlp32_Security(DHSEC_GETEXPORT,
                                   &SecurityExports);

        }
    }

    return (rc);
}

/* ******************************************************************
 *                                                                  *
 *   Strategy INIT_BASE code                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ sec32_init_base:
 *      initializes the base device driver. Since an ISS device driver
 *      is really a BASEDEV, this is similar to a BASEDEV initialization,
 *      that is, this code runs at ring 0.
 *
 *      This is called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 *
 *      This calls sec32_pre_init_base() first (assembler code).
 *
 *      In addition to the BASEDEV initialization phase, an OS/2
 *      security driver must at least issue the following:
 *
 *      1. Call the kernel DevHlp32_Security helper to retrieve 32 bits
 *         kernel SecHlp entry points.
 *
 *      2. Call the kernel DevHlp32_Security helper to pass to the kernel
 *         the addresses of our security callbacks (to hook kernel
 *         functions such as DosOpen).
 *
 *      3. Attach to SES's sesdd32.sys and call its IDC entry point to
 *         retrieve 32 bits sesdd32.sys's SecHlp entry points.
 */

int sec32_init_base(PTR16 reqpkt)
{
    int                     quiet = 0;
    int                     status;
    int                     rc;
    struct reqpkt_init      *request;
    struct ddd32_parm_list  *cmd;
    char                    *szParm;
    unsigned short          tmpsel;

    /*
     * Initialize request status
     */
    status = 0;

    /*
     * We first do preliminary init code so that DevHelp32 functions and
     * __StackToFlat() can be called.
     */
    if (    (rc = sec32_pre_init_base(reqpkt)) // assembler code
            != NO_ERROR)
    {
        // error in assembler drv32_pre_init:
        status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
    }
    else
    {
        // convert the 16:16 request packet pointer to FLAT
        if ((rc = DevHlp32_VirtToLin(reqpkt, __StackToFlat(&request)))
                    != NO_ERROR)
            // error while thunking reqpkt:
            status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
        else
        {
            // convert the 16:16 command line pointer to FLAT
            if ((rc = DevHlp32_VirtToLin(request->u.input.initarg,
                                         __StackToFlat(&cmd)))
                        != NO_ERROR)
                // error while thunking command line pointer
                status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
            else
            {
                PTR16   tmpparm;
                // OK so far: process parameters
                tmpparm.seg = request->u.input.initarg.seg;
                tmpparm.ofs = cmd->cmd_line_args;
                if ((rc = DevHlp32_VirtToLin(tmpparm, __StackToFlat(&szParm)))
                        == NO_ERROR)
                {
                    char    *tmp;
                    __strupr(szParm);
                    for (tmp = __strpbrk(szParm, "-/");
                         tmp;
                         tmp = __strpbrk(tmp, "-/"))
                    {
                        // go to char after "-" or "/"
                        tmp++;

                        // -Q: quiet initialization?
                        if (strncmp(tmp, "Q", sizeof("Q") - 1) == 0)
                        {
                            quiet = 1;
                            continue;
                        }

                        // "-AL:logfile": audit log file specified?
                        else if (strncmp(tmp, "AL", sizeof("AL") - 1) == 0)
                        {
                            if (*(tmp + 2) == ':')
                            {
                                char *pSource = tmp + 3,
                                     *pTarget = G_szLogFile;
                                while ((*pSource) && (*pSource != ' '))
                                    *pTarget++ = *pSource++;
                                *pTarget = 0;
                            }
                        }
                    }

                    if ((rc = FixMemory()) != NO_ERROR)
                        // couldn't lock 32 bits segments:
                        status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
                    else
                    {
                        // interface SES (pass kernel hooks, get helpers)
                        if ((rc = InitSecurity()) != NO_ERROR)
                        {
                            // error interfacing SES:
                            #define MSG3 "XWPSEC32.SYS: error interfacing SES."
                            DevHlp32_SaveMessage(MSG3, strlen(MSG3));
                            status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
                        }
                        else
                        {
                            ULONG ul = 0;

                            // OK, driver successfully initialized:
                            // if not "-Q", put out message
                            if (!quiet)
                            {
                                // say hello
                                DevHlp32_SaveMessage(banner,
                                                     strlen(banner) + 1);
                                // say log file name
                                if (G_szLogFile[0])
                                {
                                    _sprintf("  Audit log file is: \"%s\".\r\n",
                                             G_szLogFile);
                                    DevHlp32_SaveMessage(G_szScratchBuf,
                                                         strlen(G_szScratchBuf) + 1);
                                }
                            }

                            // and return end of code and end of data
                            // to kernel so that init code can be released
                            request->u.output.codeend = codeend;
                            request->u.output.dataend = dataend;
                        }
                    }
                }
                else
                    // error while thunking command line pointer:
                    status |= STERR + ERROR_I24_QUIET_INIT_FAIL;
            }
        }
    }
    return (status | STDON);
}

