/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_main.c	1.3 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: PR_MAIN.C
    
    Copyright (c) International Business Machines, Corp. 1994,1995

    Description: MIF Install/Uninstall handlers

    Author(s): Paul A. Ruocchio


    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        05/26/94 par    Created
        10/10/95 par    Modified for removal of BIF step.

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
#include "pr_todmi.h"
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

static short PR_mifBuild(void);

/*********************************************************************/


/************************ GLOBALS ************************************/

extern char      DH_mifDbName[];

void *OS_Install_context = NULL;

/*********************************************************************/


unsigned long PR_main(char *sourcefile,DMI_UNSIGNED *iComponentID,DMI_UNSIGNED *iIndicationType,void *OS_Context)
{
PR_ErrorClass_t      errorClass = 0;
ULONG RC;

    OS_Install_context = OS_Context;
    RC = SLERR_BAD_MIF_FILE;    /* set to the general failure case for now   */
    PR_ParserInit();            /* clean up the memory we used...           */
    if (PR_sourceOpen(sourcefile) == MIF_OKAY) {
        errorClass = PR_tokenize();
        if (errorClass == 0) errorClass = PR_parse();    
        RC = SLERR_NO_ERROR;
        PR_sourceClose();
    }
    if((RC == SLERR_NO_ERROR) && (errorClass <= PR_WARN)) {
        if (PR_mifBuild() == 0)  RC = SLERR_NO_ERROR;
        else RC = DBERR_LIMITS_EXCEEDED;   
    }
    else RC = SLERR_BAD_MIF_FILE;
    PR_tokenTableClose();
    OS_Install_context = (void *)NULL;
    if(RC == SLERR_NO_ERROR){    /* all went well, set up the return information */
        (*iComponentID) = MIF_componentLastInstalledIdGet();   /* set up the new component ID here */
        (*iIndicationType) = DMI_INSTALL_INDICATION;           /* set this to the type of thing we did */
    }
    PR_ParserInit();            /* clean up the memory we used...           */
    return RC;
}

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


static short PR_mifBuild()
{
short RC = 0;

    if (PR_ToDmi() != BifToDmiSuccess) RC = 1;   /* now move the objects into the DB */
    return RC;
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
    return sizeof(string -> length) + alignmem(string -> length);
}
