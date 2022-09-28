/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_lex.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_lex.h
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Parser Lexical Elements Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Changed line and column to unsigned long.
        12/3/93  aip    Increased maximum literal size to 510.  This is the
                        largest MIF String that can fit into one page.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c

**********************************************************************/

#ifndef PR_LEX_H_FILE
#define PR_LEX_H_FILE

/************************ INCLUDES ***********************************/
/*********************************************************************/

/************************ CONSTANTS **********************************/

#define PR_LITERAL_MAX 508

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {PR_OUTSIDE, PR_IN_COMMENT, PR_ESCAPE_CHAR_FIRST,
              PR_IN_ESCAPE_CHAR, PR_IDENT_FIRST, PR_IN_IDENT, PR_LITERAL_FIRST,
              PR_IN_LITERAL, PR_NUMBER_FIRST, PR_IN_NUMBER, PR_SPECIAL,
              PR_WHITESPACE, PR_END} PR_LexicalState_t;

typedef enum {PR_UNKNOWN_LEXICAL_TYPE, PR_LEX_CHAR, PR_LEX_IDENT,
              PR_LEX_LITERAL, PR_LEX_NUMBER, PR_LEX_EOF} PR_LexicalType_t;

typedef struct {
            unsigned long    line;
            unsigned long    col;
            PR_LexicalType_t type;
            char             value[PR_LITERAL_MAX];
        } PR_Lexel_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

void       PR_lexicalInit(void);
PR_Lexel_t *PR_lexelGetNext(void);

/*********************************************************************/

#endif
