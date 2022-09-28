/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_parse.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_parse.h
    

    Description: MIF top-level parse routines header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_parse.h 1.9 1994/08/08 15:00:50 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
         7/23/93 aip    Added prototype for PR_atol for octal and hexadecimal
                        support.  Removed old name and id prototypes.
         2/28/94 aip    Changed prototype of PR_pDescription to allow
                        "backfilling" of MIF_String pointers in the BIF
                        structures.
        3/22/94  aip    4.3
        4/21/94  aip    Added date and 64-bit types.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        10/28/94 aip    Changed revision to SDK 1.0a

**********************************************************************/

#ifndef PR_PARSE_H_FILE
#define PR_PARSE_H_FILE

/************************ INCLUDES ***********************************/

#include <stdarg.h>
#include "pr_plib.h"
#include "pr_tok.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PARSER_VERSION "1.0a"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {PR_UNKNOWN_SCOPE, PR_ATTRIBUTE_SCOPE,  PR_COMPONENT_SCOPE,
              PR_ENUM_SCOPE, PR_GROUP_SCOPE, PR_PATH_SCOPE,
              PR_TABLE_SCOPE} PR_Scope_t;

typedef struct {
    unsigned short word[4];
} PR_Int64_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

void             PR_parseInit(void);
PR_Int64_t       *PR_asciiToInt64(char *number);
unsigned long    PR_atol(char *number);
void             PR_databaseCheck(PR_Token_t *t, MIF_Pos_t pos);
PR_Int64_t       *PR_negation64(PR_Int64_t *int64);
PR_ErrorClass_t  PR_parse(void);
PR_ParseStruct_t *PR_pDescription(MIF_Pos_t *literalPos);
PR_ParseStruct_t *PR_pIdentification(char *format, ...);

/*********************************************************************/

#endif
