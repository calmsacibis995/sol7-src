/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_tok.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_tok.h
    

    Description: MIF Parser Tokenizer Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Changed keyword structure:  value is 32 bits.
                            line and column are unsigned long.
                        Changed rev of token table to 1.1
        8/1/94   aip    Moved private prototypes from .h to .c
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_TOK_H_FILE
#define PR_TOK_H_FILE

/************************ INCLUDES ***********************************/

#include "db_int.h"
#include "mif_db.h"
#include "pr_err.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_TOKEN_TABLE_MAGIC_NUMBER 26985
#define PR_TOKEN_TABLE_VERSION      "Token Table v1.1"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {PR_TOK_INITIAL, PR_TOK_AT_END_COMMENT,
              PR_TOK_AT_START_COMMENT} PR_TokenState_t;

typedef enum {PR_UNKNOWN_TOKEN_TYPE, PR_TOK_BOF, PR_TOK_CHAR, PR_TOK_IDENT,
              PR_TOK_KEYWORD, PR_TOK_LITERAL, PR_TOK_NUMBER,
              PR_TOK_EOF} PR_TokenType_t;

typedef struct {
            unsigned long  tokenNumber;
            unsigned long  line;
            unsigned long  col;
            PR_TokenType_t type;
            union {
                MIF_Int_t  iValue;
                char       *pValue;
                MIF_Pos_t  valuePos;
            } value;
            MIF_Pos_t      prev;
            MIF_Pos_t      next;
        } PR_Token_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

PR_ErrorClass_t PR_tokenize(void);
char            *PR_tokenLiteralGet(PR_Token_t *t);
MIF_Status_t    PR_tokenTableClose(void);
PR_Token_t      *PR_tokenTableGetCurrent(void);
PR_Token_t      *PR_tokenTableGetNext(void);
MIF_Bool_t      PR_tokenTableLiteralGet(char *literal, MIF_Pos_t *literalPos);
DMI_STRING      *PR_tokenTableStringGet(MIF_Pos_t literalPos);
MIF_Pos_t       PR_tokenTablePosGet(void);
MIF_Status_t    PR_tokenTablePosSet(MIF_Pos_t pos);
void            PR_tokenTableUnget(void);

/*********************************************************************/

#endif
