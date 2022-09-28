/* Copyright 10/01/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_class.c	1.3 96/10/01 Sun Microsystems"


/**********************************************************************
    Filename: pr_class.c

    Copyright (c) Intel, Inc. 1992,1993,1994,1995
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Character Class Procedures

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added support for hexadecimal numbers.
        12/22/93 aip    Cleaned up escape characters.
                        Literals now allow ISO 8859-1.
        8/29/94  aip    ANSI-compliant.
        2/6/95   aip    Allow ISO 8859-1 characters within comments.
        10/10/95 par    Modified to remove dead code

************************* INCLUDES ***********************************/

#include <string.h>
#include "pr_class.h"
#include "pr_lex.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

static char CharacterSet[]    = "\f\n\r\t\v !\"#$%&'()*+,-./0123456789:;<=>?@"
                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`"
                                "abcdefghijklmnopqrstuvwxyz{|}~";

static char EscapeCharFirstSet[] = "\"'?0123456789ABFNRTVX\\abfnrtvx";

static char EscapeCharSet[] =   "0123456789ABCDEFabcdef";

static char IdentFirstSet[]   = "ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                                "abcdefghijklmnopqrstuvwxyz";

static char IdentSet[]        = "-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_"
                                "abcdefghijklmnopqrstuvwxyz";

static char LiteralFirstSet[] = "\"";

static char NumberFirstSet[]  = "0123456789";

static char NumberSet[]       = "0123456789ABCDEFabcdef";

static char WhiteSpaceSet[]   = "\f\n\r\t\v ";


static PR_ClassTable_t ClassTable[] =
    {{PR_OUTSIDE, CharacterSet},
    {PR_IN_COMMENT, "ISO 8859-1"},
    {PR_ESCAPE_CHAR_FIRST, EscapeCharFirstSet},
    {PR_IN_ESCAPE_CHAR, EscapeCharSet},
    {PR_IDENT_FIRST, IdentFirstSet},
    {PR_IN_IDENT, IdentSet},
    {PR_LITERAL_FIRST, LiteralFirstSet},
    {PR_IN_LITERAL, "ISO 8859-1"},
    {PR_NUMBER_FIRST, NumberFirstSet},
    {PR_IN_NUMBER, NumberSet},
    {PR_WHITESPACE, WhiteSpaceSet},
    {0, (char *) 0}};

/*********************************************************************/

MIF_Bool_t PR_isClassMember(PR_LexicalState_t state, int c)
{
    int i;

/*
    For characters within literals and comments, test for ISO 8859-1 compliance
*/

    if ((state == PR_IN_LITERAL) || (state == PR_IN_COMMENT)){
        if(((c >= 0x20) && (c <= 0x7e)) || ((c >= 0xa0) && (c <= 0xff)))
		return MIF_TRUE;
	if(state == PR_IN_COMMENT && c==0x09) 
		return MIF_TRUE;
	return MIF_FALSE;
    }

/*
    Use the character tables otherwise
*/

    for (i = 0; ClassTable[i].set != (char *) 0; ++i)
        if (state == ClassTable[i].state)
            return strchr(ClassTable[i].set, c) != (char *) 0;
    return MIF_FALSE;
}


