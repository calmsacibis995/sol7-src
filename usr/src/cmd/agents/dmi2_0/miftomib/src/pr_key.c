/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_key.c	1.3 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_key.c

    Copyright (c) Intel, Inc. 1992,1993,1994,1995
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Keyword Routines

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Changed keywords to 32 bits.
                        Added support for Storage = statement.
        12/6/93  aip    Added OS keyword support.
        2/28/94  aip    Changed OS table to static
        3/22/94  aip    4.3
                        Added MACOS environment.
        8/29/94  aip    ANSI-compliant.
        10/28/94 aip    msvc-compliant.
        2/6/95   aip    Added MACOS string to PR_osKeywordToString()
        10/10/95 par    Modified to remove dead code.

************************* INCLUDES ***********************************/

#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "pr_key.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordTable_t KeywordTable[1];

/*********************************************************************/

MIF_Bool_t PR_keyValueMatch(unsigned short keyValue, char *keyword)
{
    if (keyValue == PR_keywordLookup(keyword))
        return MIF_TRUE;
    else
        return MIF_FALSE;
}

char *PR_keyValueStr(MIF_Int_t keyValue)
{
    int i;

    for (i = 0; KeywordTable[i].keyword != (char *) 0; ++i)
        if ((unsigned short) keyValue == KeywordTable[i].value)
            return KeywordTable[i].keyword;
    return (char *) 0;
}

unsigned short PR_keywordLookup(char *ident)
{
    char *p;
    int  i;

    if (islower(ident[0]))
        ident[0] = (char) toupper(ident[0]);
    p = &ident[1];
    while (*p != '\0') {
        if (isupper(*p))
            *p = (char) tolower(*p);
        ++p;
    }
    for (i = 0; KeywordTable[i].keyword != (char *) 0; ++i)
        if (strcmp(ident, KeywordTable[i].keyword) == 0)
            return KeywordTable[i].value;
    return 0;
}

MIF_Bool_t PR_keywordMatch(char *ident, char *keyword)
{
    if ((PR_keywordLookup(ident) != 0) && (strcmp(ident, keyword) == 0))
        return MIF_TRUE;
    else
        return MIF_FALSE;
}

void PR_keywordStructInit(PR_KeywordStruct_t *keywordStruct)
{
    keywordStruct -> access = PR_keywordLookup("Access");
    keywordStruct -> attribute = PR_keywordLookup("Attribute");
    keywordStruct -> class = PR_keywordLookup("Class");
    keywordStruct -> comment = PR_keywordLookup("Comment");
    keywordStruct -> component = PR_keywordLookup("Component");
    keywordStruct -> description = PR_keywordLookup("Description");
    keywordStruct -> end = PR_keywordLookup("End");
    keywordStruct -> enumeration = PR_keywordLookup("Enumeration");
    keywordStruct -> environment = PR_keywordLookup("Environment");
    keywordStruct -> group = PR_keywordLookup("Group");
    keywordStruct -> id = PR_keywordLookup("Id");
    keywordStruct -> key = PR_keywordLookup("Key");
    keywordStruct -> language = PR_keywordLookup("Language");
    keywordStruct -> name = PR_keywordLookup("Name");
    keywordStruct -> path = PR_keywordLookup("Path");
    keywordStruct -> start = PR_keywordLookup("Start");
    keywordStruct -> storage = PR_keywordLookup("Storage");
    keywordStruct -> table = PR_keywordLookup("Table");
    keywordStruct -> type = PR_keywordLookup("Type");
    keywordStruct -> value = PR_keywordLookup("Value");
    keywordStruct -> pragma = PR_keywordLookup("Pragma");


/*
    OS Keywords
*/

    keywordStruct -> dos = PR_keywordLookup("Dos");
    keywordStruct -> macos = PR_keywordLookup("Macos");
    keywordStruct -> os2 = PR_keywordLookup("Os2");
    keywordStruct -> _unix = PR_keywordLookup("Unix");
    keywordStruct -> win16 = PR_keywordLookup("Win16");
    keywordStruct -> win32 = PR_keywordLookup("Win32");
    keywordStruct -> win9x = PR_keywordLookup("Win9x");
    keywordStruct -> winnt = PR_keywordLookup("Winnt");
}

char *PR_osKeywordToString(MIF_Int_t osKeyword)
{
    static char *osStringTable[] = {"DOS", "MACOS", "OS2", "UNIX", "WIN16",
                                    "WIN32","WIN9X","WINNT"};

    if (osKeyword > PR_OS_KEYWORDS)
        return osStringTable[osKeyword - PR_OS_KEYWORDS - 1];
    else
        return (char *) 0;
}


