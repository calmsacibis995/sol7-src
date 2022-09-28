/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_plib.c	1.3 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_plib.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Parser Library

    Author(s): Alvin I. PIvowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Changed integer values to 32 bits.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
                        Changed module name to pr_plib.

************************* INCLUDES ***********************************/

#include <setjmp.h>
#include <stdarg.h>
#include <string.h>
#include "db_int.h"
#include "mif_db.h"
#include "pr_lex.h"
#include "pr_plib.h"
#include "pr_tok.h"
#include "psl_main.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static PR_ParseStruct_t *PR__parseTokenSequence(char *format, va_list argPtr);

/*********************************************************************/

/************************ GLOBALS ************************************/

static PR_ParseStruct_t ParseStruct;

/*********************************************************************/

void PR_plibInit(void)
{
    memset(&ParseStruct, 0, sizeof(PR_ParseStruct_t));
}

PR_ParseStruct_t *PR_parseAsterik(void)
{
    return PR_parseChar('*');
}

PR_ParseStruct_t *PR_parseChar(char c)
{
    return PR_parseTokenSequence("C", c);
}

PR_ParseStruct_t *PR_parseComma(void)
{
    return PR_parseChar(',');
}

PR_ParseStruct_t *PR_parseEquals(void)
{
    return PR_parseChar('=');
}

PR_ParseStruct_t *PR_parseIdent(char *ident)
{
    return PR_parseTokenSequence("I", ident);
}

PR_ParseStruct_t *PR_parseLeftBrace(void)
{
    return PR_parseChar('{');

}

PR_ParseStruct_t *PR_parseLeftBracket(void)
{
    return PR_parseChar('[');

}

PR_ParseStruct_t *PR_parseLeftParen(void)
{
    return PR_parseChar('(');

}

PR_ParseStruct_t *PR_parseLiteral(char *literal)
{
    return PR_parseTokenSequence("L", literal);
}

PR_ParseStruct_t *PR_parseKeyword(unsigned short keyword)
{
    return PR_parseTokenSequence("K", keyword);
}

PR_ParseStruct_t *PR_parseNumber(char *number)
{
    return PR_parseTokenSequence("N", number);
}

PR_ParseStruct_t *PR_parseRightBrace(void)
{
    return PR_parseChar('}');

}

PR_ParseStruct_t *PR_parseRightBracket(void)
{
    return PR_parseChar(']');

}

PR_ParseStruct_t *PR_parseRightParen(void)
{
    return PR_parseChar(')');

}

PR_ParseStruct_t *PR_parseTokenSequence(char *format, ...)
{
    va_list          argPtr;
    PR_ParseStruct_t *parseStruct;

    va_start(argPtr, format);
    parseStruct = PR__parseTokenSequence(format, argPtr);
    va_end(argPtr);
    return parseStruct;
}

PR_ParseStruct_t *PR_skipToTokenSequence(char *format, ...)
{
    va_list          argPtr;
    PR_ParseStruct_t *parseStruct;
    PR_TokenType_t   targetType;
    char             targetChar;
    char             *s;
    char             targetLiteral[PR_LITERAL_MAX + 1];
    unsigned short   targetKey;
    PR_Token_t       *token;
    PR_TokenType_t   type;
    MIF_Bool_t       match;
    MIF_Pos_t        literalPos;
    char             literal[PR_LITERAL_MAX + 1];

    va_start(argPtr, format);
    *targetLiteral = '\0';
    targetType = PR_UNKNOWN_TOKEN_TYPE;
    targetChar = '\0';
    targetKey = 0;
    switch (*format) {
        case 'c':
        case 'C':
            targetType = PR_TOK_CHAR;
            targetChar = va_arg(argPtr, char);
            break;
        case 'i':
        case 'I':
            targetType = PR_TOK_IDENT;
            s = va_arg(argPtr, char *);
            if (s != (char *) 0)
                strcpy(targetLiteral, s);
            break;
        case 'k':
        case 'K':
            targetType = PR_TOK_KEYWORD;
            targetKey = va_arg(argPtr, unsigned short);
            break;
        case 'l':
        case 'L':
            targetType = PR_TOK_LITERAL;
            s = va_arg(argPtr, char *);
            if (s != (char *) 0)
                strcpy(targetLiteral, s);
            break;
        case 'n':
        case 'N':
            targetType = PR_TOK_NUMBER;
            s = va_arg(argPtr, char *);
            if (s != (char *) 0)
                strcpy(targetLiteral, s);
            break;
    }
    va_end(argPtr);

    token = PR_tokenTableGetCurrent();
    type = token -> type;
    for (;;) {
        match = MIF_FALSE;
        if (type == targetType)
            switch (type) {
                case PR_TOK_CHAR:
                    if ((targetChar == '\0') ||
                        (targetChar == token -> value.iValue))
                        match = MIF_TRUE;
                    break;
                case PR_TOK_IDENT:
                case PR_TOK_LITERAL:
                case PR_TOK_NUMBER:
                    if (*targetLiteral != '\0') {
                        literalPos.page = token -> value.valuePos.page;
                        literalPos.offset = token -> value.valuePos.offset;
                        PR_tokenTableLiteralGet(literal, &literalPos);
                        if (strcmp(literal, targetLiteral) == 0)
                            match = MIF_TRUE;
                    } else
                        match = MIF_TRUE;
                    break;
                case PR_TOK_KEYWORD:
                    if ((targetKey == 0) ||
                        (targetKey == (unsigned short) token -> value.iValue))
                        match = MIF_TRUE;
                    break;
            }
        if (type == PR_TOK_EOF)
            PR_errorLogAdd(token -> line, token -> col, PR_FATAL +
                           PR_SOURCE_ERROR + PR_UNEXPECTED_EOF);
        if (match) {
            va_start(argPtr, format);
            parseStruct = PR__parseTokenSequence(format, argPtr);
            va_end(argPtr);
            if (PR_SUCCESSFUL(parseStruct))
                return parseStruct;
        }
        token = PR_tokenTableGetNext();
        type = token -> type;
    }
}

static PR_ParseStruct_t *PR__parseTokenSequence(char *format, va_list argPtr)
{
    MIF_Pos_t      tokenTablePos;
    PR_Token_t     *token;
    PR_TokenType_t type;
    MIF_Int_t      iValue;
    MIF_Bool_t     optionalFlag,SuccessFlag;
    MIF_Pos_t      literalPos;
    char           literal[PR_LITERAL_MAX + 1];
    PR_Token_t     *t;
    char           c;
    unsigned short key;
    char           *s;

    ParseStruct.result = PR_SUCCEED;
    SuccessFlag = TRUE;
    ParseStruct.tokenCount = 0;
    if (*format == '\0')
        return &ParseStruct;
    if (strlen(format) > PR_TOKEN_MAX) {
        ParseStruct.result = PR_FAIL;
        return &ParseStruct;
    }

    tokenTablePos = PR_tokenTablePosGet();
    token = PR_tokenTableGetCurrent();
    while ((*format != '\0') && (SuccessFlag == TRUE)) {
        type = token -> type;
        optionalFlag = MIF_FALSE;
        switch (*format) {
            case 'c':
                optionalFlag = MIF_TRUE;
            case 'C':                     /* Fall Through */
                literalPos.page = token -> value.valuePos.page;
                literalPos.offset = token -> value.valuePos.offset;
                if((type != PR_TOK_KEYWORD) && (type != PR_TOK_EOF))
                    PR_tokenTableLiteralGet(literal, &literalPos);
                c = va_arg(argPtr, char);
                if (((type == PR_TOK_CHAR) &&
                    ((c == '\0') || (*literal == c))) ||
                    (! optionalFlag)) {
                    t = &ParseStruct.token[ParseStruct.tokenCount];
                    t -> tokenNumber = token -> tokenNumber;
                    t -> line = token -> line;
                    t -> col = token -> col;
                    t -> type = type;
                    t -> value.valuePos.page = token -> value.valuePos.page;
                    t -> value.valuePos.offset = token -> value.valuePos.offset;
                    ++ParseStruct.tokenCount;
                    token = PR_tokenTableGetNext();
                }
                if (((type != PR_TOK_CHAR) ||
                    ((c != '\0') && (*literal != c))) &&
                    (! optionalFlag))
                    SuccessFlag = FALSE;    /* bug out of here */
                break;
            case 'i':
                optionalFlag = MIF_TRUE;
            case 'I':                     /* Fall Through */
                literalPos.page = token -> value.valuePos.page;
                literalPos.offset = token -> value.valuePos.offset;
                if((type != PR_TOK_KEYWORD) && (type != PR_TOK_EOF))
                    PR_tokenTableLiteralGet(literal, &literalPos);
                s = va_arg(argPtr, char *);
                if (((type == PR_TOK_IDENT) &&
                    ((s == (char *) 0) || (strcmp(s, literal) == 0))) ||
                    (! optionalFlag)) {
                    t = &ParseStruct.token[ParseStruct.tokenCount];
                    t -> tokenNumber = token -> tokenNumber;
                    t -> line = token -> line;
                    t -> col = token -> col;
                    t -> type = type;
                    t -> value.valuePos.page = token -> value.valuePos.page;
                    t -> value.valuePos.offset = token -> value.valuePos.offset;
                    ++ParseStruct.tokenCount;
                    token = PR_tokenTableGetNext();
                }
                if (((type != PR_TOK_IDENT) ||
                    ((s != (char *) 0) && (strcmp(s, literal) != 0))) &&
                    (! optionalFlag))
                    SuccessFlag = FALSE;    /* bug out of here */
                break;
            case 'k':
                optionalFlag = MIF_TRUE;
            case 'K':                    /* Fall Through */
                iValue = token -> value.iValue;
                key = va_arg(argPtr, unsigned short);
                if (((type == PR_TOK_KEYWORD) &&
                    ((key == 0) || ((unsigned short) iValue == key))) ||
                    (! optionalFlag)) {
                    t = &ParseStruct.token[ParseStruct.tokenCount];
                    t -> tokenNumber = token -> tokenNumber;
                    t -> line = token -> line;
                    t -> col = token -> col;
                    t -> type = type;
                    t -> value.iValue = iValue;
                    ++ParseStruct.tokenCount;
                    token = PR_tokenTableGetNext();
                }
                if (((type != PR_TOK_KEYWORD) ||
                    ((key != 0) && ((unsigned short) iValue != key))) &&
                    (! optionalFlag))
                    SuccessFlag = FALSE;    /* bug out of here */
                break;
            case 'l':
                optionalFlag = MIF_TRUE;
            case 'L':                     /* Fall Through */
                literalPos.page = token -> value.valuePos.page;
                literalPos.offset = token -> value.valuePos.offset;
                if((type != PR_TOK_KEYWORD) && (type != PR_TOK_EOF))
                    PR_tokenTableLiteralGet(literal, &literalPos);
                s = va_arg(argPtr, char *);
                if (((type == PR_TOK_LITERAL) &&
                    ((s == (char *) 0) || (strcmp(s, literal) == 0))) ||
                    (! optionalFlag)) {
                    t = &ParseStruct.token[ParseStruct.tokenCount];
                    t -> tokenNumber = token -> tokenNumber;
                    t -> line = token -> line;
                    t -> col = token -> col;
                    t -> type = type;
                    t -> value.valuePos.page = token -> value.valuePos.page;
                    t -> value.valuePos.offset = token -> value.valuePos.offset;
                    ++ParseStruct.tokenCount;
                    token = PR_tokenTableGetNext();
                }
                if (((type != PR_TOK_LITERAL) ||
                    ((s != (char *) 0) && (strcmp(s, literal) != 0))) &&
                    (! optionalFlag))
                    SuccessFlag = FALSE;    /* bug out of here */
                break;
            case 'n':
                optionalFlag = MIF_TRUE;
            case 'N':                     /* Fall Through */
                literalPos.page = token -> value.valuePos.page;
                literalPos.offset = token -> value.valuePos.offset;
                if((type != PR_TOK_KEYWORD) && (type != PR_TOK_EOF))
                    PR_tokenTableLiteralGet(literal, &literalPos);
                s = va_arg(argPtr, char *);
                if (((type == PR_TOK_NUMBER) &&
                    ((s == (char *) 0) || (strcmp(s, literal) == 0))) ||
                    (! optionalFlag)) {
                    t = &ParseStruct.token[ParseStruct.tokenCount];
                    t -> tokenNumber = token -> tokenNumber;
                    t -> line = token -> line;
                    t -> col = token -> col;
                    t -> type = type;
                    t -> value.valuePos.page = token -> value.valuePos.page;
                    t -> value.valuePos.offset = token -> value.valuePos.offset;
                    ++ParseStruct.tokenCount;
                    token = PR_tokenTableGetNext();
                }
                if (((type != PR_TOK_NUMBER) ||
                    ((s != (char *) 0) && (strcmp(s, literal) != 0))) &&
                    (! optionalFlag))
                    SuccessFlag = FALSE;    /* bug out of here */
                break;
            default:
                SuccessFlag = FALSE;    /* bug out of here */
                break;
        }
        format++;
    }
    if (SuccessFlag == FALSE) {   /* the parser failed for some reason */
        PR_tokenTablePosSet(tokenTablePos);
        ParseStruct.result = PR_FAIL;
    }
    return &ParseStruct;
}
