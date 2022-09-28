/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_tok.c	1.4 96/10/02 Sun Microsystems"


/**********************************************************************
    Filename: pr_tok.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Tokenizer Routines

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Removed compound keyword handling.
        7/28/93  aip    Added casts to unsigned short where appropriate.
        8/5/93   aip    Added casts to unsigned short where appropriate.
        11/2/93  aip    Added low-level debug code to help find hanging bugs.
        8/1/94   aip    Moved private prototypes from .h to .c
        10/10/95 par    Modified to remove dead code, and to be memory resident

************************* INCLUDES ***********************************/

#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "os_svc.h"
#include "mif_db.h"
#include "pr_err.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_src.h"
#include "pr_tok.h"

/*********************************************************************/

/************************** Structures *******************************/

typedef struct Table {
    struct Table    *Next;            /* if more then one of these is in use */
    unsigned long   offset;           /* working offset for this block */
}TableStr, *TablePtr;

#define TOKEN_BLOCK_SIZE  512         /* size of the block allocated for the token parsing */

/************************ PRIVATE ************************************/

static PR_Token_t    *PR_tokenGetNext(void);
static void          PR_tokenTableBuild(void);
static PR_Token_t    *PR_tokenTableTokenGet(MIF_Pos_t tokenPos);
static MIF_Pos_t     PR_tokAlloc(unsigned short Size);
static void          *PR_tokResolve(MIF_Pos_t tokenPos);
static void          *PR_tokAllocBlock(void);
static unsigned long PR_tokCalcNextPage(TablePtr CurrentPage);

/*********************************************************************/

/************************ GLOBALS ************************************/

TablePtr        TableBase = NULL;           /* initialize the table base to NULL */
TablePtr        TableEnd  = NULL;           /* initialize the end of the table to NULL */
MIF_Pos_t       TokenTablePos;              /* base token table pos */
PR_TokenState_t TokenState;

/*********************************************************************/

static unsigned long PR_tokCalcNextPage(TablePtr CurrentPage)
{
    return (unsigned long)CurrentPage->Next;   /* return the pointer to the next page */
}

static void *PR_tokAllocBlock(void)
{
TablePtr New;

    New = OS_Alloc(sizeof(TableStr) + TOKEN_BLOCK_SIZE);   /* grab a block of memory here */
    if(New == (TablePtr)NULL) return (void *)NULL;     /* unable to allocate a block here */
    New->offset = 0;            /* preset the offset to zero, this is where we will start allocating */
    New->Next = (TablePtr)NULL;          /* terminate the chain here */
    return (void *)New;
}

static MIF_Pos_t PR_tokAlloc(unsigned short Size)
{
	TablePtr New,Slot;
	static MIF_Pos_t pos;

    pos.page = 0;
    pos.offset = 0;
    if(TableBase == (TablePtr)NULL){  /* this is the base of the table */
        TokenState = PR_TOK_INITIAL;    /* set the initial state here */
        New = (TablePtr)PR_tokAllocBlock();
        if(New == (TablePtr)NULL) return pos;
        TableBase = Slot = TableEnd = New;              /* slot is the working ptr to do the allocation from */
    }
    else Slot = TableEnd;   /* point to the current end of the table */
    /* now we have a block, now find the space in the block */
    if((Slot->offset + Size) > TOKEN_BLOCK_SIZE){  /* we need to allocate a new block */
        Slot->offset = TOKEN_BLOCK_SIZE;
        New = (TablePtr)PR_tokAllocBlock();
        if(New == (TablePtr)NULL) return pos;
        Slot->Next = New;
        TableEnd = Slot = New;                          /* slot is the working ptr to do the allocation from */
    }
    pos.page = (unsigned long)Slot;    /* assign out pointer as the page number - it makes resolve fly... */
    pos.offset = (unsigned short)Slot->offset;
    Slot->offset += Size;      /* increment this for the next time in here */
    return pos;
}

static void *PR_tokResolve(MIF_Pos_t tokenPos)
{
TablePtr Work;

    Work = (TablePtr)tokenPos.page;   /* point to the table element we want to work with here */
    if(Work != (TablePtr)NULL)   /* we have a pointer... */
        return (void *) (((char *)(Work + 1)) + tokenPos.offset);
    return (void *)NULL;
}

PR_ErrorClass_t PR_tokenize(void)
{
    jmp_buf fatalException;

    if (setjmp(fatalException) == 0) {
        PR_errorFatalExceptionTrapSet(&fatalException);
        PR_tokenTableBuild();
    }
    return PR_errorLargestErrorClassEncounteredGet();
}

static PR_Token_t *PR_tokenGetNext(void)
{
    static PR_Lexel_t      *lexel;
    static unsigned long   tokenNumber;
    static PR_Token_t      token;
    static char            value[PR_LITERAL_MAX + 1];
    unsigned short         key;
    static MIF_Semaphore_t commentSemaphore;

    lexel = PR_lexelGetNext();
    for (;;) {
        switch (TokenState) {
            case PR_TOK_INITIAL:
                token.tokenNumber = ++tokenNumber;
                token.line = lexel -> line;
                token.col = lexel -> col;
                token.value.pValue = value;
                switch (lexel -> type) {
                    case PR_LEX_CHAR:
                        token.type = PR_TOK_CHAR;
                        strcpy(value, lexel -> value);
                        if (commentSemaphore == 0)
                            return &token;
                        else
                            lexel = PR_lexelGetNext();
                        break;
                    case PR_LEX_IDENT:
                        key = PR_keywordLookup(lexel -> value);
                        if (key != 0) {
                            token.type = PR_TOK_KEYWORD;
                            token.value.iValue = key;
                        } else {
                            token.type = PR_TOK_IDENT;
                            strcpy(value, lexel -> value);
                        }
                        if (commentSemaphore == 0)
                            return &token;
                        else
                            lexel = PR_lexelGetNext();
                        break;
                    case PR_LEX_LITERAL:
                        token.type = PR_TOK_LITERAL;
                        strcpy(value, lexel -> value);
                        if (commentSemaphore == 0)
                            return &token;
                        else
                            lexel = PR_lexelGetNext();
                        break;
                    case PR_LEX_NUMBER:
                        token.type = PR_TOK_NUMBER;
                        strcpy(value, lexel -> value);
                        if (commentSemaphore == 0)
                            return &token;
                        else
                            lexel = PR_lexelGetNext();
                        break;
                    case PR_LEX_EOF:
                        if (commentSemaphore != 0)
                            PR_errorLogAdd(lexel -> line, lexel -> col,
                                           PR_FATAL + PR_COMMENT_ERROR +
                                           PR_UNEXPECTED_EOF);
                        token.type = PR_TOK_EOF;
                        value[0] = '\0';
                        return &token;
                }
                break;
            case PR_TOK_AT_END_COMMENT:
                if (commentSemaphore == 0)
                    PR_errorLogAdd(lexel -> line, lexel -> col, PR_ERROR +
                                   PR_COMMENT_ERROR +
                                   PR_UNEXPECTED_END_OF_COMMENT);
                else
                    --commentSemaphore;
                lexel = PR_lexelGetNext();
                TokenState = PR_TOK_INITIAL;
                break;
            case PR_TOK_AT_START_COMMENT:
                ++commentSemaphore;
                lexel = PR_lexelGetNext();
                TokenState = PR_TOK_INITIAL;
                break;
        }
    }
}

char *PR_tokenLiteralGet(PR_Token_t *t)
{
    MIF_Pos_t   literalPos;
    static char literal[PR_LITERAL_MAX + 1];

    literalPos.page = t -> value.valuePos.page;
    literalPos.offset = t -> value.valuePos.offset;
    PR_tokenTableLiteralGet(literal, &literalPos);
    return literal;
}

static void PR_tokenTableBuild(void)
{
    PR_Token_t            *token;
    static unsigned long  tokenNumber;
    PR_TokenType_t        tokenType;
    MIF_Pos_t             tokenPos;
    MIF_Pos_t             literalPos;
    PR_Token_t            *t;
    char                  *c;
    char                  *p;
    MIF_Pos_t             prevTokenPos;
    static MIF_Pos_t      prevLiteralPos;
    static PR_TokenType_t prevType;

    TokenTablePos = prevTokenPos = PR_tokAlloc(alignmem(sizeof(PR_Token_t)));
    if ((TokenTablePos.page == 0) && (TokenTablePos.offset == 0))
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    if ((t = (PR_Token_t *)PR_tokResolve(TokenTablePos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    t -> tokenNumber = tokenNumber++;
    t -> line = 0;
    t -> col = 0;
    t -> type = prevType = PR_TOK_BOF;
    t -> value.iValue = 0;
    t -> prev.page = 0;
    t -> prev.offset = 0;

    token = PR_tokenGetNext();
    tokenType = token -> type;
    while (tokenType != PR_TOK_EOF) {
        if ((prevType == PR_TOK_LITERAL) && (tokenType == PR_TOK_LITERAL)) {
            if ((c = (char *)PR_tokResolve(prevLiteralPos)) == (char *) 0)
                PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
            p = token -> value.pValue;
            *c++ = *p++;
            while (*p != '\0') {
                prevLiteralPos = PR_tokAlloc(alignmem(1));
                if ((c = (char *) PR_tokResolve(prevLiteralPos)) == (char *) 0)
                    PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
                *c++ = *p++;
            }
            prevLiteralPos = PR_tokAlloc(alignmem(1));
            if ((c = (char *) PR_tokResolve(prevLiteralPos)) == (char *) 0)
                PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
            *c = '\0';
        } else {
            tokenPos = PR_tokAlloc(sizeof(PR_Token_t));
            if ((tokenPos.page == 0) && (tokenPos.offset == 0))
                PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
            switch (tokenType) {
                case PR_TOK_CHAR:
                case PR_TOK_IDENT:
                case PR_TOK_LITERAL:
                case PR_TOK_NUMBER:
                    literalPos = PR_tokAlloc(alignmem((unsigned short)(strlen(token -> value.pValue)+1)));
                    if ((literalPos.page == 0) && (literalPos.offset == 0))
                        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
                    break;
                case PR_TOK_KEYWORD:
                case PR_TOK_EOF:
                    break;
            }
    
            if ((t = (PR_Token_t *) PR_tokResolve(tokenPos)) == (PR_Token_t *) 0)
                PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
            t -> tokenNumber = tokenNumber++;
            t -> line = token -> line;
            t -> col = token -> col;
            t -> type = token -> type;
            t -> prev.page = prevTokenPos.page;
            t -> prev.offset = prevTokenPos.offset;
            t -> next.page = 0;
            t -> next.offset = 0;
            switch (tokenType) {
                case PR_TOK_CHAR:
                case PR_TOK_IDENT:
                case PR_TOK_LITERAL:
                case PR_TOK_NUMBER:
                    t -> value.valuePos.page = literalPos.page;
                    t -> value.valuePos.offset = literalPos.offset;
                    if ((c = (char *) PR_tokResolve(literalPos)) == (char *) 0)
                        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
                    strcpy(c, token -> value.pValue);
                    prevLiteralPos.page = literalPos.page;
                    prevLiteralPos.offset = (unsigned short)
                                            (literalPos.offset + strlen(c));
                    break;
                case PR_TOK_KEYWORD:
                    t -> value.iValue = token -> value.iValue;
                    break;
                case PR_TOK_EOF:
                    t -> value.iValue = 0;
                    break;
            }
    
            if ((t = (PR_Token_t *) PR_tokResolve(prevTokenPos)) == (PR_Token_t *) 0)
                PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
            t -> next.page = tokenPos.page;
            t -> next.offset = tokenPos.offset;

            prevTokenPos.page = tokenPos.page;
            prevTokenPos.offset = tokenPos.offset;
        }
        token = PR_tokenGetNext();
        prevType = tokenType;
        tokenType = token -> type;
    }

    tokenPos = PR_tokAlloc(sizeof(PR_Token_t));
    if ((tokenPos.page == 0) && (tokenPos.offset == 0))
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    if ((t = (PR_Token_t *) PR_tokResolve(tokenPos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    t -> tokenNumber = tokenNumber++;
    t -> line = token -> line;
    t -> col = token -> col;
    t -> type = PR_TOK_EOF;
    t -> value.iValue = 0;
    t -> prev.page = prevTokenPos.page;
    t -> prev.offset = prevTokenPos.offset;
    t -> next.page = 0;
    t -> next.offset = 0;

    if ((t = (PR_Token_t *) PR_tokResolve(prevTokenPos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    t -> next.page = tokenPos.page;
    t -> next.offset = tokenPos.offset;
}

MIF_Status_t PR_tokenTableClose(void)
{
TablePtr Next,Work;

    Work = TableBase;    /* start with the base here */
    while (Work != (TablePtr)NULL) {
        Next = Work->Next;   /* save the next ptr first */
        OS_Free(Work);       /* free the block */
        Work = Next;         /* move on to the next sucker */
    } /* endwhile */
    TableBase = NULL;        /* reset the base ptr, so it will be ready for the next time */
    TableEnd = NULL;
    return MIF_OKAY;
}

PR_Token_t *PR_tokenTableGetCurrent(void)
{
    PR_Token_t *t;

    if ((t = PR_tokenTableTokenGet(TokenTablePos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    return t;
}

PR_Token_t *PR_tokenTableGetNext(void)
{
    PR_Token_t *t;

    if ((t = (PR_Token_t *) PR_tokResolve(TokenTablePos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    if ((t -> next.page != 0) || (t -> next.offset != 0)) {
        TokenTablePos.page = t -> next.page;
        TokenTablePos.offset = t -> next.offset;
    }

    if ((t = PR_tokenTableTokenGet(TokenTablePos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    return t;
}

MIF_Bool_t PR_tokenTableLiteralGet(char *literal, MIF_Pos_t *literalPos)
{
    char        *c;
    int         i;

    if (literal == (char *) 0)
        return MIF_FALSE;
    *literal = '\0';
    if ((literalPos -> page == 0) && (literalPos -> offset == 0))
        return MIF_FALSE;

    if ((c = (char *) PR_tokResolve(*literalPos)) == (char *) 0)
        return MIF_FALSE;
    for (i = 0; (*c != '\0') && (i < PR_LITERAL_MAX); ++i) {
        literal[i] = *c++;
        ++literalPos -> offset;
        if (literalPos -> offset == TOKEN_BLOCK_SIZE) {
            literalPos->page = PR_tokCalcNextPage((TablePtr)literalPos->page);
            literalPos -> offset = 0;
            if ((c = (char *) PR_tokResolve(*literalPos)) == (char *) 0) {
                literal[i] = '\0';
                return MIF_FALSE;
            }
        }
    }
    literal[i] = '\0';
    return (*c != '\0');
}

DMI_STRING *PR_tokenTableStringGet(MIF_Pos_t literalPos)
{
    DMI_STRING  *string;
    static char dmiString[TOKEN_BLOCK_SIZE + 1];
    char        buffer[PR_LITERAL_MAX + 1];
    size_t      length;
    MIF_Bool_t  moreLiteral;

    if ((literalPos.page == 0) && (literalPos.offset == 0))
        return (DMI_STRING *) 0;

    string = (DMI_STRING *) dmiString;
    string -> length = 0;
    string -> body[0] = '\0';

    do {
        moreLiteral = PR_tokenTableLiteralGet(buffer, &literalPos);
        if (strlen(buffer) + strlen(string -> body) >
            TOKEN_BLOCK_SIZE - sizeof(string -> length)) {
            length = TOKEN_BLOCK_SIZE - sizeof(string -> length) -
                     strlen(string -> body);
            moreLiteral = MIF_FALSE;
        } else
            length = strlen(buffer);
        strncat(string -> body, buffer, length);
        string -> length += length;
    } while (moreLiteral);

    return string;
}

MIF_Pos_t PR_tokenTablePosGet(void)
{
    return TokenTablePos;
}

MIF_Status_t PR_tokenTablePosSet(MIF_Pos_t pos)
{
    TokenTablePos.page = pos.page;
    TokenTablePos.offset = pos.offset;
    return MIF_OKAY;
}

static PR_Token_t *PR_tokenTableTokenGet(MIF_Pos_t tokenPos)
{
    static PR_Token_t token;
    PR_Token_t        *t;

    if ((tokenPos.page == 0) && (tokenPos.offset == 0))
        return (PR_Token_t *) 0;
    if ((t = (PR_Token_t *) PR_tokResolve(tokenPos)) == (PR_Token_t *) 0)
        return (PR_Token_t *) 0;
    token.tokenNumber = t -> tokenNumber;
    token.line = t -> line;
    token.col = t -> col;
    token.type = t -> type;
    switch (token.type) {
        case PR_TOK_CHAR:
        case PR_TOK_IDENT:
        case PR_TOK_LITERAL:
        case PR_TOK_NUMBER:
            token.value.valuePos = t -> value.valuePos;
            break;
        case PR_TOK_KEYWORD:
            token.value.iValue = t -> value.iValue;
            break;
        case PR_TOK_EOF:
            token.value.iValue = 0;
            break;
    }
    return &token;
}

void PR_tokenTableUnget(void)
{
    PR_Token_t *t;

    if ((t = (PR_Token_t *)
        PR_tokResolve(TokenTablePos)) == (PR_Token_t *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_FILE_ERROR);
    if ((t -> prev.page != 0) || (t -> prev.offset != 0)) {
        TokenTablePos.page = t -> prev.page;
        TokenTablePos.offset = t -> prev.offset;
    }
}

