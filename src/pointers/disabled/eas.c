// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// OS/2 Toolkit
#define INCL_ERRORS
#define INCL_PM
#define INCL_WIN
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

// generic headers
#include "setup.h"				// code generation and debugging options

#include "pointers\eas.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

// set to 1 to activate
#define USE_EAMVMT 1

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WriteStringEa                                              ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 13.04.1998                                                 ³
 *³ Update    : 13.04.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : HFILE - handle to file                                     ³
 *³             PSZ   - name of EA                                         ³
 *³             PSZ   - string value of ea                                 ³
 *³ Tasks     : - write ea                                                 ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */


APIRET _Export WriteStringEa
 (
	 HFILE hfile,
	 PSZ pszEaName,
	 PSZ pszEaValue
)
{

	APIRET rc = NO_ERROR;
	PFEA2LIST pfea2l = NULL;
	PEASVST peasvst;
	PEAMVMT peamvmt;
	ULONG ulEAListLen;
	ULONG ulValueLen;
	EAOP2 eaop2;
	PSZ pszValue;

	do
	{
		// check parameters
		if ((hfile == NULLHANDLE) ||
			(pszEaName == NULL) ||
			(*pszEaName == 0) ||
			(pszEaValue == NULL))
		{
			rc = ERROR_INVALID_PARAMETER;
			break;
		}

		// write EAs
		ulValueLen = strlen(pszEaValue) +

#ifdef USE_EAMVMT
			sizeof(EAMVMT);
#else
			sizeof(EASVST);
#endif

		ulEAListLen = strlen(pszEaName) +
			sizeof(FEA2LIST) +
			ulValueLen;

		// get memory for FEA2LIST
		if ((pfea2l = malloc(ulEAListLen)) == 0)
		{
			rc = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}

		// init FEA2LIST
		eaop2.fpGEA2List = NULL;
		eaop2.fpFEA2List = pfea2l;
		memset(pfea2l, 0, ulEAListLen);

		// write timeframe EA
		pfea2l->cbList = ulEAListLen;
		pfea2l->list[0].cbName = strlen(pszEaName);
		strcpy(pfea2l->list[0].szName, pszEaName);

		// delete attribute if value empty
		if (strlen(pszEaValue) == 0)
			pfea2l->list[0].cbValue = 0;
		else
		{

			pfea2l->list[0].cbValue = ulValueLen;

#ifdef USE_EAMVMT
			// multi value multi type
			peamvmt = NEXTSTR(pfea2l->list[0].szName);
			peamvmt->usType = EAT_MVMT;
			peamvmt->usCodepage = 0;
			peamvmt->usEntries = 1;
			peamvmt->usEntryType = EAT_ASCII;
			peamvmt->usEntryLen = strlen(pszEaValue);
			memcpy(&peamvmt->chEntry[0], pszEaValue, peamvmt->usEntryLen);
#else
			// single value single type
			peasvst = NEXTSTR(pfea2l->list[0].szName);
			peasvst->usType = EAT_ASCII;
			peasvst->usEntryLen = strlen(pszEaValue);
			memcpy(&peasvst->chEntry[0], pszEaValue, peasvst->usEntryLen);
#endif
		}

		// set the new EA value
		rc = DosSetFileInfo(hfile,
							FIL_QUERYEASIZE,
							&eaop2,
							sizeof(eaop2));

	}
	while (FALSE);

// cleanup
	if (pfea2l)
		free(pfea2l);
	return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ReadStringEa                                               ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 13.04.1998                                                 ³
 *³ Update    : 13.04.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : PSZ   - name of file                                       ³
 *³             PSZ   - name of EA                                         ³
 *³             PSZ   - buffer                                             ³
 *³             ULONG - buffer len                                         ³
 *³ Tasks     : - write ea                                                 ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */


APIRET _Export ReadStringEa
 (
	 PSZ pszFileName,
	 PSZ pszEaName,
	 PSZ pszBuffer,
	 PULONG pulBuflen
)
{

	APIRET rc = NO_ERROR;
	FILESTATUS4 fs4;

	EAOP2 eaop2;
	PGEA2LIST pgea2l = NULL;
	PFEA2LIST pfea2l = NULL;

	PGEA2 pgea2;
	PFEA2 pfea2;

	ULONG ulGea2Len = 0;
	ULONG ulFea2Len = 0;

	PEASVST peasvst;
	PEAMVMT peamvmt;

	ULONG ulRequiredLen;

	do
	{
		// check parameters
		if ((pszFileName == NULL) ||
			(pszEaName == NULL) ||
			(*pszEaName == 0) ||
			(pulBuflen == NULL))
		{
			rc = ERROR_INVALID_PARAMETER;
			break;
		}

		// initialize target buffer
		if (pszBuffer)
			memset(pszBuffer, 0, *pulBuflen);

		// get EA size
		rc = DosQueryPathInfo(pszFileName,
							  FIL_QUERYEASIZE,
							  &fs4,
							  sizeof(fs4));
		if (rc != NO_ERROR)
			break;

		// no eas here ?
		if (fs4.cbList == 0)
		{
			pulBuflen = 0;
			break;
		}

		// determine required space
		// - for ulFea2Len use at least 2 * Gea2Len because
		//   buffer needs at least to be Geal2Len even for an empty
		//   attribute, otherwise rc == ERROR_BUFFER_OVERFLOW !
		ulGea2Len = sizeof(GEA2LIST) + strlen(pszEaName);
		ulFea2Len = 2 * MAX(fs4.cbList, ulGea2Len);

		// get memory for GEA2LIST
		if ((pgea2l = malloc(ulGea2Len)) == 0)
		{
			rc = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}
		memset(pgea2l, 0, ulGea2Len);

		// get memory for FEA2LIST
		if ((pfea2l = malloc(ulFea2Len)) == 0)
		{
			rc = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}
		memset(pfea2l, 0, ulFea2Len);

		// init ptrs and do the query
		memset(&eaop2, 0, sizeof(EAOP2));
		eaop2.fpGEA2List = pgea2l;
		eaop2.fpFEA2List = pfea2l;
		pfea2l->cbList = ulFea2Len;
		pgea2l->cbList = ulGea2Len;

		pgea2 = &pgea2l->list[0];
		pfea2 = &pfea2l->list[0];


		pgea2->oNextEntryOffset = 0;
		pgea2->cbName = strlen(pszEaName);
		strcpy(pgea2->szName, pszEaName);

		rc = DosQueryPathInfo(pszFileName,
							  FIL_QUERYEASFROMLIST,
							  &eaop2,
							  sizeof(eaop2));
		if (rc != NO_ERROR)
			break;

		// check first entry only
		peamvmt = (PEAMVMT) ((PBYTE) pfea2->szName + pfea2->cbName + 1);

		// is it MVMT ? then adress single EA !
		if (peamvmt->usType == EAT_MVMT)
		{
			peasvst = (PEASVST) & peamvmt->usEntryType;
		}
		else
			peasvst = (PEASVST) peamvmt;


		// is it ASCII ?
		if (peasvst->usType != EAT_ASCII)
		{
			rc = ERROR_INVALID_DATA;
			break;
		}

		// check buffer and hand over value
		ulRequiredLen = peasvst->usEntryLen + 1;
		if (*pulBuflen < ulRequiredLen)
		{
			*pulBuflen = ulRequiredLen;
			rc = ERROR_BUFFER_OVERFLOW;
			break;
		}

		// hand over len
		*pulBuflen = ulRequiredLen;

		// hand over value
		if (pszBuffer)
			memcpy(pszBuffer, peasvst->chEntry, peasvst->usEntryLen);

	}
	while (FALSE);

// cleanup
	if (pgea2l)
		free(pgea2l);
	if (pfea2l)
		free(pfea2l);
	return rc;
}
