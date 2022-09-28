/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_parse.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_parse.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF top-level parsing routines

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_parse.c 1.20 1994/08/08 14:56:51 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added new identification parsing procedure.
                        Removed old name and id parsing routines.
        12/3/93  aip    Disallow names greater than 256 characters.
                        Do not allow static tables outside of component scope.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit strings.
        4/21/94  aip    Added date and 64-bit types.
        5/19/94  aip    Changed return type of PR_parse() to PR_ErrorClass_t.
        6/10/94  aip    Don't add zero-length descriptions to the database.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/4/94   aip    Keep descriptions in token table.
        8/20/94  par    Fixed 64 bit numerics

************************* INCLUDES ***********************************/

#include <ctype.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "pr_comp.h"
#include "pr_err.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_plib.h"
#include "pr_table.h"
#include "os_svc.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static PR_Int64_t     *PR_add64(PR_Int64_t *addend1, PR_Int64_t *addend2);
static unsigned short PR_errorSubjectGet(void);
static PR_Int64_t     *PR_shortToInt64(short value);
static void           PR_skipScope(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

PR_KeywordStruct_t      Ks;
PR_Scope_t              Scope;

static PR_ParseStruct_t Identification;

/*********************************************************************/

void PR_parseInit(void)
{
    PR_keywordStructInit(&Ks);
    Scope = PR_UNKNOWN_SCOPE;
    memset(&Identification, 0 , sizeof(PR_ParseStruct_t));
}

PR_ErrorClass_t PR_parse(void)
{
    jmp_buf            fatalException;
    PR_ParseStruct_t   *ps;
    PR_Token_t         *t;

    if (setjmp(fatalException) != 0)
        return PR_FATAL;
    PR_errorFatalExceptionTrapSet(&fatalException);

/*
    Move past BOF
*/

    PR_tokenTableGetNext();

/*
    Skip over optional language statement.
*/

    PR_parseTokenSequence("kcl", Ks.language, '=', (char *) 0);

/*
        Start Component
*/
 
    if (PR_SUCCESSFUL(ps = PR_parseTokenSequence("KK", Ks.start, Ks.component)))
        PR_pComponent();
    else {
        t = &(ps -> token[0]);
        PR_errorLogAdd(t -> line, t -> col, (PR_ErrorNumber_t)
            (PR_ERROR + PR_COMPONENT_ERROR + PR_EXPECTING_START));

/*
        End Component
*/

        PR_skipToTokenSequence("KK", Ks.end, Ks.component);
    }

/*
    EOF
*/

    t = PR_tokenTableGetNext();
    if (t -> type != PR_TOK_EOF)
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_ERROR + PR_SOURCE_ERROR + PR_EXPECTING_END));

    return PR_errorLargestErrorClassEncounteredGet();
}

PR_Int64_t *PR_add64(PR_Int64_t *addend1, PR_Int64_t *addend2)
{
short    carry,i;
unsigned long     mySum;
static PR_Int64_t sum;

    carry = 0;
#ifdef _M_I86
    for (i = 0; i < 4; ++i) {
#else
    for (i = 3; i >= 0; --i) {
#endif
        mySum = (unsigned long) (addend1->word[i] + addend2->word[i] + carry);
        sum.word[i]= (unsigned short)(0x0000FFFFL & mySum);             
        carry = (short)(0x00000001L & (mySum>>16));                     
    }                                                                   
    return &sum;                                                        

}

PR_Int64_t *PR_asciiToInt64(char *number)
{
PR_Int64_t     addend,New64;
unsigned short digit;
short            i;
static PR_Int64_t     int64;
short            radix;
#ifdef _M_I86
#   define LSW 0
#else
#   define LSW 3
#endif

    memset(&int64, 0, sizeof(PR_Int64_t));
    if (*number == '0') {
        ++number;
        if ((*number == 'x') || (*number == 'X')) {
            ++number;
            radix = 16;
        }
        else radix = 8;
    }
    else radix = 10;
    while (*number != '\0') {
        memcpy(&addend, &int64, sizeof(PR_Int64_t));
        for (i = 1; i < radix; ++i) memcpy(&int64, PR_add64(&int64, &addend), sizeof(PR_Int64_t));
        digit = *number++;
        if (isdigit(digit)) digit -= '0';
        else {
            if (islower(digit)) digit = (unsigned short) toupper(digit);
            digit -= ('A' - 10);
        }
        memset(&New64, 0, sizeof(PR_Int64_t));
        New64.word[LSW] = digit;
        memcpy(&int64, PR_add64(&int64,&New64),sizeof(PR_Int64_t));
    }
    return &int64;
}

unsigned long PR_atol(char *number)
{
    unsigned short radix;
    unsigned long  value;
    unsigned short digit;

    value = 0;

    if (*number == '0') {
        ++number;
        if ((*number == 'x') || (*number == 'X')) {
            ++number;
            radix = 16;
        } else
            radix = 8;
    } else
        radix = 10;

    while (*number != '\0') {
        if (*number <= '9')
            digit = (unsigned short) (*number - '0');
        else if (islower(*number))
            digit = (unsigned short) (*number - 'a' + 10);
        else
            digit = (unsigned short) (*number - 'A' + 10);
        value = radix * value + digit;
        ++number;
    }

    return value;
}

void PR_databaseCheck(PR_Token_t *t, MIF_Pos_t pos)
{
    if ((pos.page != 0) || (pos.offset != 0))
        return;

    PR_errorLogAdd(t -> line, t -> col, (PR_ErrorNumber_t)
        (PR_FATAL + PR_errorSubjectGet() + PR_DATABASE_FAULT));
}

static unsigned short PR_errorSubjectGet(void)
{
    switch (Scope) {
        case PR_ATTRIBUTE_SCOPE:
            return PR_ATTRIBUTE_ERROR;
        case PR_COMPONENT_SCOPE:
            return PR_COMPONENT_ERROR;
        case PR_ENUM_SCOPE:
            return PR_ENUM_ERROR;
        case PR_GROUP_SCOPE:
            return PR_GROUP_ERROR;
        case PR_PATH_SCOPE:
            return PR_PATH_ERROR;
        case PR_TABLE_SCOPE:
            return PR_TABLE_ERROR;
    }
}

PR_Int64_t *PR_negation64(PR_Int64_t *int64)
{
    static PR_Int64_t result64;
    int               i;

    memcpy(&result64, int64, sizeof(PR_Int64_t));
    for (i = 0; i < 4; ++i)
        result64.word[i] = (unsigned short) (~ result64.word[i]);
    memcpy(&result64, PR_add64(int64, PR_shortToInt64(1)), sizeof(PR_Int64_t));
    return &result64;
}

PR_ParseStruct_t *PR_pDescription(MIF_Pos_t *literalPos)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.description))) {
        if (PR_UNSUCCESSFUL(ps = PR_parseEquals())) {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_EQUALS;
            goto descriptionError;
        }
        ps = PR_parseLiteral((char *) 0);
        t = &(ps -> token[0]);
        if (PR_UNSUCCESSFUL(ps)) {
            errorCode = PR_EXPECTING_LITERAL;
            goto descriptionError;
        }
        if ((literalPos -> page != 0) || (literalPos -> offset != 0)) {
            errorCode = PR_DUPLICATE_STATEMENT;
            goto descriptionError;
        }
        literalPos -> page = t -> value.valuePos.page;
        literalPos -> offset = t -> value.valuePos.offset;
    }
    return ps;

descriptionError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_errorSubjectGet() + errorCode));
    return ps;
}

PR_ParseStruct_t *PR_pIdentification(char *format, ...)
{
    typedef struct {
                MIF_Bool_t     keywordFound;
                unsigned short keyword;
            } keywordTable_t;

    va_list          argPtr;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;
    PR_ErrorNumber_t errorCode;
    int              keywordCount;
    keywordTable_t   *keywordTable;
    MIF_Bool_t       keywordFound;
    char             *literal;
    size_t           literalLength;
    MIF_Int_t        value;
    int              p;
    int              i;

    t = (PR_Token_t *) 0;
    ps = (PR_ParseStruct_t *) 0;

    keywordCount = (int) strlen(format);
    keywordTable = (keywordTable_t *)
                       OS_Alloc(keywordCount * sizeof(keywordTable_t));
    if (keywordTable == (keywordTable_t *) 0)
        PR_errorLogAdd(0, 0, (PR_ErrorNumber_t)
            (PR_FATAL + PR_errorSubjectGet() + PR_OUT_OF_MEMORY));
    va_start(argPtr, format);
    for (i = 0; i < keywordCount; ++i) {
        keywordTable[i].keywordFound = MIF_FALSE;
        keywordTable[i].keyword = va_arg(argPtr, unsigned short);
        Identification.token[i].tokenNumber = 0;
    }
    va_end(argPtr);

    p = 0;
    for (;;) {
        keywordFound = MIF_FALSE;
        for (i = 0; i < keywordCount; ++i)
            if (PR_SUCCESSFUL(ps = PR_parseKeyword(keywordTable[i].keyword))) {
                keywordFound = MIF_TRUE;
                if (keywordTable[i].keywordFound) {
                    errorCode = PR_DUPLICATE_STATEMENT;
                    goto identificationError;
                }
                if (PR_SUCCESSFUL(ps = PR_parseEquals())) {
                    switch (format[i]) {
                        case 'I':
                        case 'i':
                            if (PR_SUCCESSFUL(
                                   ps = PR_parseNumber((char *) 0))) {
                                t = &(ps -> token[0]);
                                literal = PR_tokenLiteralGet(t);
                                value = PR_atol(literal);
                                if (value == 0) {
                                    errorCode = PR_ILLEGAL_VALUE;
                                    goto identificationError;
                                }
                            } else {
                                errorCode = PR_EXPECTING_INTEGER;
                                goto identificationError;
                            }
                            break;
                        case 'L':
                        case 'l':
                            if (PR_SUCCESSFUL(
                                   ps = PR_parseLiteral((char *) 0))) {
                                t = &(ps -> token[0]);
                                literal = PR_tokenLiteralGet(t);
                                literalLength = strlen(literal);
                                if ((literalLength == 0) ||
                                    (literalLength > 256)) {
                                    errorCode = PR_ILLEGAL_VALUE;
                                    goto identificationError;
                                }
                            } else {
                                errorCode = PR_EXPECTING_LITERAL;
                                goto identificationError;
                            }
                            break;
                    }
                    Identification.tag[p] = (unsigned short) i;
                    Identification.token[p].tokenNumber = t -> tokenNumber;
                    Identification.token[p].line = t -> line;
                    Identification.token[p].col = t -> col;
                    Identification.token[p].type = t -> type;
                    Identification.token[p].value.valuePos.page =
                        t -> value.valuePos.page;
                    Identification.token[p].value.valuePos.offset =
                        t -> value.valuePos.offset;
                    keywordTable[i].keywordFound = MIF_TRUE;
                } else {
                    errorCode = PR_EXPECTING_EQUALS;
                    goto identificationError;
                }
                break;
            }
        if (! keywordFound) {
            for (i = 0; i < keywordCount; ++i)
                if (isupper(format[i]) && ! keywordTable[i].keywordFound) {
                    errorCode = PR_EXPECTING_IDENTIFICATION;
                    goto identificationError;
                }
            OS_Free(keywordTable);
            Identification.result = PR_SUCCEED;
            Identification.tokenCount = (unsigned short) p;
            return &Identification;
        }
        ++p;
    }

identificationError:

    OS_Free(keywordTable);
    t = &(ps -> token[0]);
    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_errorSubjectGet() + errorCode));
    PR_tokenTableUnget();
    PR_skipScope();
    Identification.result = PR_FAIL;
    return &Identification;
}

static PR_Int64_t *PR_shortToInt64(short value)
{
    static PR_Int64_t int64;

    memset(&int64, 0, sizeof(PR_Int64_t));
    int64.word[0] = value;
    return &int64;
}

static void PR_skipScope(void)
{
    switch(Scope) {
        case PR_ATTRIBUTE_SCOPE:
            PR_skipToTokenSequence("KK", Ks.end, Ks.attribute);
            break;
        case PR_COMPONENT_SCOPE:
            PR_skipToTokenSequence("KK", Ks.end, Ks.component);
            break;
        case PR_ENUM_SCOPE:
            PR_skipToTokenSequence("KK", Ks.end, Ks.enumeration);
            break;
        case PR_GROUP_SCOPE:
            PR_skipToTokenSequence("KK", Ks.end, Ks.group);
            break;
        case PR_PATH_SCOPE:
            PR_skipToTokenSequence("KK", Ks.end, Ks.path);
            break;
    }
}

