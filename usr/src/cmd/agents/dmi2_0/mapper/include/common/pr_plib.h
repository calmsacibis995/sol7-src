/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_plib.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_plib.h
    

    Description: MIF Parser Library Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        2/28/94  aip    Added tag array to PR_ParseStruct
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
                        Changed module name to pr_plib.

**********************************************************************/

#ifndef PR_PLIB_H_FILE
#define PR_PLIB_H_FILE

/************************ INCLUDES ***********************************/

#include <stdarg.h>
#include "pr_tok.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_SUCCESSFUL(ps)   ((ps) -> result)
#define PR_UNSUCCESSFUL(ps) (! (PR_SUCCESSFUL(ps)))
#define PR_TOKEN_MAX        10

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {PR_FAIL, PR_SUCCEED} PR_ParseResult_t;

typedef struct {
            PR_ParseResult_t result;
            unsigned short   tokenCount;
            unsigned short   tag[PR_TOKEN_MAX];
            PR_Token_t       token[PR_TOKEN_MAX];
        } PR_ParseStruct_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

void             PR_plibInit(void);
PR_ParseStruct_t *PR_parseAsterik(void);
PR_ParseStruct_t *PR_parseChar(char c);
PR_ParseStruct_t *PR_parseComma(void);
PR_ParseStruct_t *PR_parseEquals(void);
PR_ParseStruct_t *PR_parseIdent(char *ident);
PR_ParseStruct_t *PR_parseKeyword(unsigned short keyword);
PR_ParseStruct_t *PR_parseLeftBrace(void);
PR_ParseStruct_t *PR_parseLeftBracket(void);
PR_ParseStruct_t *PR_parseLeftParen(void);
PR_ParseStruct_t *PR_parseLiteral(char *literal);
PR_ParseStruct_t *PR_parseNumber(char *number);
PR_ParseStruct_t *PR_parseRightBrace(void);
PR_ParseStruct_t *PR_parseRightBracket(void);
PR_ParseStruct_t *PR_parseRightParen(void);
PR_ParseStruct_t *PR_parseTokenSequence(char *format, ...);
PR_ParseStruct_t *PR_skipToTokenSequence(char *format, ...);

/*********************************************************************/
#endif
