/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_err.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_err.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Parser Error Logging Procedures

    Author(s): Alvin I. Pivowar
               Paul A. Ruochio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Changed line and column to unsigned longs.
        7/28/93  aip    Added casts to unsigned short where appropriate.
        11/10/93 aip    Fixed bug that mangled error log when traversing page
                        boundaries.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/23/94  par    Modified for OS independence.
        10/10/95 par    Modified to remove dead code

************************* INCLUDES ***********************************/

#include <ctype.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pr_class.h"
#include "pr_err.h"
#include "os_svc.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

extern void             *OS_Install_context;

static jmp_buf          *FatalExceptionTrap;
static PR_ErrorClass_t  LargestErrorClassEncountered;
static MIF_Bool_t       ErrorLogCreation;
static MIF_Pos_t        ErrorLogPos;
static MIF_Pos_t        ErrorLogEndPos;

/*********************************************************************/

void PR_errorInit(void)
{
    LargestErrorClassEncountered = 0;
    ErrorLogCreation = MIF_FALSE;
    ErrorLogPos.page = 0;
    ErrorLogPos.offset = 0;
    ErrorLogEndPos.page = 0;
    ErrorLogEndPos.offset = 0;
}

unsigned short PR_errorClass(PR_ErrorNumber_t errorNumber)
{
    return (unsigned short) (1000 * (errorNumber / 1000));
}

void PR_errorFatalExceptionTrapSet(jmp_buf *fatalException)
{
    FatalExceptionTrap = fatalException;
}

PR_ErrorClass_t PR_errorLargestErrorClassEncounteredGet(void)
{
    return LargestErrorClassEncountered;
}

void PR_errorLargestErrorClassEncounteredSet(PR_ErrorClass_t errorClass)
{
    if (errorClass > LargestErrorClassEncountered)
        LargestErrorClassEncountered = errorClass;
}

MIF_Status_t PR_errorLogAdd(unsigned long line, unsigned long col,
                            PR_ErrorNumber_t errorNumber)
{
PR_ErrorClass_t errorClass   = PR_errorClass(errorNumber);
char buffer[256];

    /* Get OS-dependent error message, reserving space at end of buffer for
       carriage return / line feed. */
    OS_GetParserMessage(errorNumber, line, col, buffer, 240);
    /* Ask the OS to notify the user of the error. */
    OS_InstallNotice(buffer, OS_Install_context, (DMI_MgmtCommand_t *) 0);
    PR_errorLargestErrorClassEncounteredSet(errorClass);
    if (errorClass == PR_FATAL) longjmp(*FatalExceptionTrap, PR_FATAL);
    else return MIF_OKAY;
}

