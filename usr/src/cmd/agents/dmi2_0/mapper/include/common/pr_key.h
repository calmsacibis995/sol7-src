/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_key.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_key.h
    

    Description: MIF Parser Keyword Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added Storage keyword.
                        Removed Common and Specific keyword.
                        Changed keywords to 32 bits.
        12/6/93  aip    Added OS keyword support.
        3/22/94  aip    4.3
                        Added MACOS environment.
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_KEY_H_FILE
#define PR_KEY_H_FILE

/************************ INCLUDES ***********************************/

#include "db_int.h"
#include "mif_db.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_OS_KEYWORDS 1000

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef struct {
            char           *keyword;
            unsigned short value;
        } PR_KeywordTable_t;

typedef struct {
            unsigned short access;
            unsigned short attribute;
            unsigned short class;
            unsigned short comment;
            unsigned short component;
            unsigned short description;
            unsigned short end;
            unsigned short enumeration;
            unsigned short environment;
            unsigned short group;
            unsigned short id;
            unsigned short key;
            unsigned short language;
            unsigned short name;
            unsigned short path;
            unsigned short start;
            unsigned short storage;
            unsigned short table;
            unsigned short type;
            unsigned short value;
            unsigned short pragma;
/*
            OS Keywords
*/

            unsigned short dos;
            unsigned short macos;
            unsigned short os2;
            unsigned short _unix;
            unsigned short win16;
            unsigned short win32;
            unsigned short win9x;
            unsigned short winnt;
        } PR_KeywordStruct_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

MIF_Bool_t     PR_keyValueMatch(unsigned short keyValue, char *keyword);
char           *PR_keyValueStr(MIF_Int_t keyValue);
unsigned short PR_keywordLookup(char *ident);
MIF_Bool_t     PR_keywordMatch(char *ident, char *keyword);
void           PR_keywordStructInit(PR_KeywordStruct_t *keywordStruct);
char           *PR_osKeywordToString(MIF_Int_t keyValue);

/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#endif
