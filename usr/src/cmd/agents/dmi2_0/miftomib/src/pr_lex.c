/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_lex.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_lex.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Parser Lexical Elements Procedures

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added support for octal and hexadecimal numbers.
        8/19/93  aip    Fixed files terminated with a ^Z.
        9/9/93   aip    Fixed '/' handling.
        11/10/93 aip    Test for illegal characters within literals.
        12/3/93  aip    Disallow ; style comments.
        12/22/93 aip    Cleaned up escape characters.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c

************************* INCLUDES ***********************************/

#include <ctype.h>
#include <stdio.h>
#include "pr_class.h"
#include "pr_err.h"
#include "pr_lex.h"
#include "pr_src.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static MIF_Bool_t PR_isLegalDigit(unsigned short radix, int c);

/*********************************************************************/

/************************ GLOBALS ************************************/

static PR_LexicalState_t LexicalState;
static unsigned short    Line;
static unsigned short    Col;

/*********************************************************************/

void PR_lexicalInit(void)
{
    LexicalState = PR_OUTSIDE;
    Line = 0;
    Col = 0;
}

PR_Lexel_t *PR_lexelGetNext(void)
{
    int                c;
    int                d;
    unsigned short     radix;
    static PR_Lexel_t  lexel;
    unsigned short     index;
    unsigned short     startCol;
    unsigned short     escapeCol;
    unsigned short     escapeCount;
    unsigned short     escapeRadix;
    unsigned short     escapeValue;

    radix = 0;
    index = 0;
    startCol = 0;
    escapeCol = 0;
    escapeCount = 0;
    escapeRadix = 0;
    escapeValue = 0;

    if (Line == 0)
        ++Line;

    c = PR_sourceCharGetNext();
    ++Col;
    for (;;) {
        switch (LexicalState) {
            case PR_OUTSIDE:
                if (c == '/') {
                    d = PR_sourceCharGetNext();
                    ++Col;
                    if (d == '/')
                        LexicalState = PR_IN_COMMENT;
                    else {
                        PR_sourceCharGetPrev();
                        --Col;
                        LexicalState = PR_SPECIAL;
                    }
                } else if (PR_isClassMember(PR_IDENT_FIRST, c))
                    LexicalState = PR_IDENT_FIRST;
                else if (PR_isClassMember(PR_LITERAL_FIRST, c))
                    LexicalState = PR_LITERAL_FIRST;
                else if (PR_isClassMember(PR_NUMBER_FIRST, c))
                    LexicalState = PR_NUMBER_FIRST;
                else if (PR_isClassMember(PR_WHITESPACE, c))
                    LexicalState = PR_WHITESPACE;
                else if (c == EOF)
                    LexicalState = PR_END;
                else if (PR_isClassMember(PR_OUTSIDE, c))
                    LexicalState = PR_SPECIAL;
                else {

/*
                    Test for ^Z
*/

                    if (c == 0x1a) {
                        LexicalState = PR_END;
                        break;
                    }
                    PR_errorLogAdd(Line, Col, PR_ERROR + PR_SOURCE_ERROR +
                                   PR_ILLEGAL_CHARACTER);
                    c = PR_sourceCharGetNext();
                    ++Col;
                }
                break;
            case PR_IN_COMMENT:
                if ((c == '\n') || (c == EOF))
                    LexicalState = PR_WHITESPACE;
                else if (! PR_isClassMember(PR_IN_COMMENT, c)) {
                    PR_errorLogAdd(Line, Col, PR_WARN + PR_COMMENT_ERROR +
                                   PR_ILLEGAL_CHARACTER);
                    c = PR_sourceCharGetNext();
                    ++Col;
                } else {
                    c = PR_sourceCharGetNext();
                    ++Col;
                }
                break;
            case PR_ESCAPE_CHAR_FIRST:
                escapeCol = (unsigned short) (Col - 1);
                escapeCount = 0;
                escapeValue = 0;
                if (PR_isClassMember(PR_ESCAPE_CHAR_FIRST, c)) {
                    switch (c) {
                        case 'a':
                        case 'A':
                            lexel.value[index++] = PR_ASCII_BEL;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'b':
                        case 'B':
                            lexel.value[index++] = PR_ASCII_BS;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'f':
                        case 'F':
                            lexel.value[index++] = PR_ASCII_FF;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'n':
                        case 'N':
                            lexel.value[index++] = PR_ASCII_LF;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'r':
                        case 'R':
                            lexel.value[index++] = PR_ASCII_CR;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 't':
                        case 'T':
                            lexel.value[index++] = PR_ASCII_HT;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'v':
                        case 'V':
                            lexel.value[index++] = PR_ASCII_VT;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case 'x':
                        case 'X':
                            escapeRadix = 16;
                            LexicalState = PR_IN_ESCAPE_CHAR;
                            break;
                        case '\'':
                        case '"':
                        case '\\':
                        case '?':
                            lexel.value[index++] = (char) c;
                            LexicalState = PR_IN_LITERAL;
                            break;
                        case '0':
                            escapeRadix = 0;
                            LexicalState = PR_IN_ESCAPE_CHAR;
                            break;
                    }
                } else {
                    PR_errorLogAdd(Line, Col, PR_ERROR + PR_ESCAPE_CHAR_ERROR +
                                   PR_ILLEGAL_CHARACTER);   
                    LexicalState = PR_IN_LITERAL;
                }
                c = PR_sourceCharGetNext();
                ++Col;
                break;
            case PR_IN_ESCAPE_CHAR:
                if ((((escapeRadix == 16) && (escapeCount <= 4)) ||
                     ((escapeRadix == 8) && (escapeCount <= 6)) ||
                     (escapeRadix ==0)) &&
                    PR_isClassMember(PR_IN_ESCAPE_CHAR, c)) {
                    if (escapeRadix == 0)
                        if ((c == 'x') || (c == 'X'))
                            escapeRadix = 16;
                        else {
                            escapeRadix = 8;
                            PR_sourceCharGetPrev();
                            --Col;
                        }
                    else {
                       if (! isdigit(c))
                           if (islower(c))
                               c = c - 'a' + 10;
                           else
                               c = c - 'A' + 10;
                       else
                           c = c - '0';
                       if (c >= (int) escapeRadix) {
                           PR_errorLogAdd(Line, Col, PR_ERROR +
                                          PR_ESCAPE_CHAR_ERROR +
                                          PR_ILLEGAL_DIGIT);
                           LexicalState = PR_IN_LITERAL;
                       }
                       ++escapeCount;
                       escapeValue = (unsigned short)
                           (escapeRadix * escapeValue + c);
                       c = PR_sourceCharGetNext();
                    }
                } else {
                    if (escapeCount == 0)
                        PR_errorLogAdd(Line, Col, PR_ERROR +
                                       PR_ESCAPE_CHAR_ERROR + PR_MISSING_DIGIT);
                    else if (escapeValue > PR_ASCII_MAX)
                        PR_errorLogAdd(Line, escapeCol, PR_ERROR +
                                       PR_ESCAPE_CHAR_ERROR + PR_ILLEGAL_VALUE);
                    else
                        lexel.value[index++] = (char) escapeValue;
                    LexicalState = PR_IN_LITERAL;
                }
                break;
            case PR_IDENT_FIRST:
                startCol = Col;
                index = 0;
                lexel.value[index] = (char) c;
                LexicalState = PR_IN_IDENT;
                c = PR_sourceCharGetNext();
                ++Col;
                break;
            case PR_IN_IDENT:
                if (PR_isClassMember(PR_IN_IDENT, c)) {
                    lexel.value[++index] = (char) c;
                    c = PR_sourceCharGetNext();
                    ++Col;
                } else {
                    PR_sourceCharGetPrev();
                    --Col;
                    lexel.line = Line;
                    lexel.col = startCol;
                    lexel.type = PR_LEX_IDENT;
                    lexel.value[++index] = '\0';
                    LexicalState = PR_OUTSIDE;
                    return &lexel;
                }
                break;
            case PR_LITERAL_FIRST:
                startCol = Col;
                index = 0;
                LexicalState = PR_IN_LITERAL;
                c = PR_sourceCharGetNext();
                ++Col;
                break;
            case PR_IN_LITERAL:
                if (PR_isClassMember(PR_LITERAL_FIRST, c)) {
                    lexel.line = Line;
                    lexel.col = startCol;
                    lexel.type = PR_LEX_LITERAL;
                    lexel.value[index] = '\0';
                    LexicalState = PR_OUTSIDE;
                    return &lexel;
                } else if (PR_isClassMember(PR_IN_LITERAL, c)) {
                    if (c == '\\')
                        LexicalState = PR_ESCAPE_CHAR_FIRST;
                    else
                        lexel.value[index++] = (char) c;
                    c = PR_sourceCharGetNext();
                    ++Col;
                } else if (c == '\n') {
                    LexicalState = PR_WHITESPACE;
                    PR_errorLogAdd(Line, Col, PR_ERROR + PR_LITERAL_ERROR +
                                   PR_UNEXPECTED_NEW_LINE);
                } else if (c == EOF) {
                    LexicalState = PR_WHITESPACE;
                    PR_errorLogAdd(Line, Col, PR_FATAL + PR_LITERAL_ERROR +
                                   PR_UNEXPECTED_EOF);
                } else {
                    PR_errorLogAdd(Line, Col, PR_ERROR + PR_LITERAL_ERROR +
                                   PR_ILLEGAL_CHARACTER);
                    c = PR_sourceCharGetNext();
                    ++Col;
                }
                break;
            case PR_NUMBER_FIRST:
                startCol = Col;
                index = 0;
                if (c == '0')
                    radix = 8;
                else
                    radix = 10;
                lexel.value[index++] = (char) c;
                LexicalState = PR_IN_NUMBER;
                c = PR_sourceCharGetNext();
                ++Col;
                break;
            case PR_IN_NUMBER:
                if ((PR_isClassMember(PR_IN_NUMBER, c) &&
                     PR_isLegalDigit(radix, c)) ||
                    (((c == 'x') || (c == 'X')) && (index == 1))) {
                    if ((c == 'x') || (c == 'X'))
                        radix = 16;
                    lexel.value[index++] = (char) c;
                    c = PR_sourceCharGetNext();
                    ++Col;
                } else {
                    PR_sourceCharGetPrev();
                    --Col;
                    lexel.line = Line;
                    lexel.col = startCol;
                    lexel.type = PR_LEX_NUMBER;
                    lexel.value[index] = '\0';
                    LexicalState = PR_OUTSIDE;
                    return &lexel;
                }
                break;
            case PR_SPECIAL:
                lexel.line = Line;
                lexel.col = Col;
                lexel.type = PR_LEX_CHAR;
                lexel.value[0] = (char) c;
                lexel.value[1] = '\0';
                LexicalState = PR_OUTSIDE;
                return &lexel;
            case PR_WHITESPACE:
                if (PR_isClassMember(PR_WHITESPACE, c)) {
                    if (c == '\n') {
                        ++Line;
                        Col = 0;
                    }
                    c = PR_sourceCharGetNext();
                    ++Col;
                } else
                    LexicalState = PR_OUTSIDE;
                break;
            case PR_END:
                lexel.line = Line;
                lexel.col = Col;
                lexel.type = PR_LEX_EOF;
                lexel.value[0] = '\0';
                return &lexel;
        }
    }
}

static MIF_Bool_t PR_isLegalDigit(unsigned short radix, int c)
{
    if (c <= '9')
        return ((c - '0') < (int) radix);

    if (islower(c))
        c = c - 'a' + 10;
    else
        c = c - 'A' + 10;

    return (c < (int) radix);
}
