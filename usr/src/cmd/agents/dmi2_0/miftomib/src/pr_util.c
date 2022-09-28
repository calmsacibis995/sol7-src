/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_util.c	1.3 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: PR_MAIN.C -- relevant routines to MIFTOMIB $MED

    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Install/Uninstall handlers

    Author(s): Paul A. Ruocchio


    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        05/26/94 par    Created

************************* INCLUDES ***********************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psl_util.h"
#include "os_svc.h"
#include "db_api.h"
#include "pr_main.h"
#include "pr_err.h"
#include "pr_parse.h"
#include "pr_src.h"
#include "pr_tok.h"
/*#include BIF_CODE_GENERATOR_HEADER $MED */
#include "pr_attr.h"
#include "pr_lex.h"
#include "pr_path.h"
#include "pr_comp.h"
#include "pr_enum.h"
#include "pr_group.h"
#include "pr_table.h"

/*********************************************************************/
void PR_ParserInit(void);  /* frees up any memory that may have been allocated, and resets structs */

/************************ PRIVATE ************************************/

static short PR_mifBuild(char *bifFile, char *databaseFile);

/*********************************************************************/


/************************ GLOBALS ************************************/

FILE             *BIF;
extern char      DH_mifDbName[];

void *OS_Install_context = NULL;


void PR_ParserInit(void)  /* frees up any memory that may have been allocated, and resets structs */
{
    PR_attributeInit();
    PR_tableInit();
    PR_plibInit();
    PR_pathsInit();
    PR_parseInit();
    PR_lexicalInit();
    PR_groupInit();
    PR_errorInit();
    PR_enumerationInit();
    PR_componentInit();
}

/************************ String Procedures **************************/

short PR_stringCmp(DMI_STRING *string1, DMI_STRING *string2)
{
    if((string1 == (DMI_STRING *)NULL) || (string2 == (DMI_STRING *)NULL)) return -1;
    return memcmp(string1, string2, (size_t) (sizeof(string1 -> length) +
                  string1 -> length));
}

DMI_STRING *PR_stringInit(DMI_STRING *destination, char *source)
{
    destination -> length = strlen(source);
    memcpy(&destination -> body, source, (size_t) destination -> length);
    return destination;
}

unsigned long PR_stringSize(DMI_STRING *string)
{
    if(string == (DMI_STRING *)NULL) return 0;
    return sizeof(string -> length) + string -> length;
}

/* the rest is stolen from pr_err.c $MED */
static char             ErrorLogFilename[FILENAME_MAX];
static MIF_FileHandle_t ErrorLogHandle;
static MIF_Bool_t       ErrorLogCreation;
static MIF_Pos_t        ErrorLogPos;
static MIF_Pos_t        ErrorLogEndPos;

MIF_Status_t PR_errorLogClose(void)
{
    long size;

    if (ErrorLogCreation) {
        size = MIF_PAGE_SIZE * ErrorLogPos.page + ErrorLogPos.offset;
/*//$M    if (MIF_chsize(ErrorLogHandle, size) != MIF_OKAY)
//$M        return MIF_FILE_ERROR;*/
    }
    if (MIF_close(ErrorLogHandle) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if ((ErrorLogPos.page == 0) && (ErrorLogPos.offset == 0))
        if (remove(ErrorLogFilename) == -1)
            return MIF_FILE_ERROR;
    return MIF_OKAY;
}

MIF_Status_t PR_errorLogCreate(char *filename)
{
    strcpy(ErrorLogFilename, filename);
    ErrorLogHandle = MIF_create(ErrorLogFilename, 0, (char *) 0);
    if (ErrorLogHandle == 0)
        return MIF_FILE_ERROR;
    else {
        ErrorLogCreation = MIF_TRUE;
        return MIF_OKAY;
    }
}

