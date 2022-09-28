/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_path.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_path.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser path routines

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_path.c 1.5 1994/08/08 15:03:53 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Use new identification parsing.
        12/6/93  aip    New path block format.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit strings.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved prototypes from .h to .c
        10/10/95 par    Modified for the removal of dead code.

************************* INCLUDES ***********************************/

#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "mif_db.h"
#include "pr_todmi.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_path.h"
#include "pr_plib.h"
#include "os_svc.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static void PR_dPaths(void);
static void PR_ePaths(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

static struct PathData {
    struct PathData             *next;
    DMI_STRING                  *pSymbolName;
	struct EnvironmentData {
         DMI_STRING             *pPathName;
         MIF_PathType_t         pathType;
         unsigned long          iEnvironmentId;
         unsigned long          osPathname;
    } environmentData[BIF_LAST_OS + 1];
} PathData;

/*********************************************************************/

void PR_pathsInit(void)
{
    struct PathData     *p;
    BIF_OsEnvironment_t os;

    while (PathData.next != (struct PathData *) 0) {
        p = PathData.next;
        if (p -> pSymbolName != (DMI_STRING *) 0)
            OS_Free(p -> pSymbolName);
        for (os = BIF_FIRST_OS; os <= BIF_LAST_OS; ++os)
            if (p -> environmentData[os].pPathName != (DMI_STRING *) 0)
                OS_Free(p -> environmentData[os].pPathName);
        PathData.next = p -> next;
        OS_Free(p);
    }
    memset(&PathData,0,sizeof(struct PathData));
}

void PR_pPaths(void)
{
    PR_ErrorNumber_t       errorCode;
    PR_ParseStruct_t       *ps;
    PR_Token_t             *t;
    char                   *literal;
    struct PathData        *p;
    struct EnvironmentData *e;
    unsigned long          environmentId;
    MIF_Bool_t             foundStatement;
    PR_uString             (string, PR_LITERAL_MAX);
    unsigned long          stringSize;
    int                    i;

    Scope = PR_PATH_SCOPE;

/*
    name = <literal>
*/

    if (PR_UNSUCCESSFUL(ps = PR_pIdentification("L", Ks.name)))
        return;
    t = &(ps -> token[0]);
    literal = PR_tokenLiteralGet(t);
    PR_stringInit((DMI_STRING *) string, literal);
    stringSize = PR_stringSize((DMI_STRING *) string);

/*
    Add name to list if it is not already there
*/

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (PR_stringCmp(p -> pSymbolName, (DMI_STRING *) string) == 0)
            break;
    
    if (p == (struct PathData *) 0) {
        for (p = &PathData; p -> next != (struct PathData *) 0; p = p -> next)
            ;
        if ((p -> next =
             (struct PathData *) OS_Alloc(sizeof(struct PathData))) ==
             (struct PathData *) 0) {
            errorCode = PR_OUT_OF_MEMORY;
            goto pathFatal;
        }
        p = p -> next;
        p -> next = (struct PathData *) 0;
        if ((p -> pSymbolName = (DMI_STRING *) OS_Alloc((size_t) stringSize)) ==
            (DMI_STRING *) 0) {
            errorCode = PR_OUT_OF_MEMORY;
            goto pathFatal;
        }
        memcpy(p -> pSymbolName, string, (size_t) stringSize);
        for (i = 1; i <= BIF_LAST_OS; ++i) {
            p -> environmentData[i].pPathName = (DMI_STRING *) 0;
            p -> environmentData[i].iEnvironmentId = 0;
        }
    }
    
/*
    <keyword> = <literal>
    <keyword> = Direct-Interface
*/

    foundStatement = MIF_FALSE;
    while (PR_SUCCESSFUL(ps = PR_parseTokenSequence("KC", 0, '='))) {
        t = &(ps -> token[0]);
        if (t -> value.iValue < PR_OS_KEYWORDS) {
            errorCode = PR_EXPECTING_OS;
            goto pathError;
        }

        environmentId = t -> value.iValue - PR_OS_KEYWORDS;
        PR_stringInit((DMI_STRING *) string,
                      PR_osKeywordToString(t -> value.iValue));
        stringSize = PR_stringSize((DMI_STRING *) string);

/*
    Add OS to environmentData array if it was not previously defined
*/

        e = &p -> environmentData[environmentId];
        if (e -> iEnvironmentId == 0)
            e -> iEnvironmentId = environmentId;
        else {
            errorCode = PR_CONFLICTING;
            goto pathError;
        }

/*
    Parse path
*/

        if (PR_SUCCESSFUL(ps = PR_parseLiteral((char *) 0))) {
            e -> pathType = MIF_OVERLAY_PATH_TYPE;
            t = &(ps -> token[0]);
            literal = PR_tokenLiteralGet(t);
            PR_stringInit((DMI_STRING *) string, literal);
            stringSize = PR_stringSize((DMI_STRING *) string);
            if (strlen(literal) == 0) {
                errorCode = PR_ILLEGAL_VALUE;
                goto pathError;
            }
            if ((e -> pPathName = (DMI_STRING *)
                                  OS_Alloc((size_t) stringSize)) ==
                (DMI_STRING *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto pathFatal;
            }
            memcpy(e -> pPathName, string, (size_t) stringSize);
        } else if (PR_SUCCESSFUL(ps = PR_parseIdent("Direct-interface")))
            e -> pathType = MIF_DIRECT_INTERFACE_PATH_TYPE;
        else {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_VALUE;
            goto pathError;
        }

        foundStatement = MIF_TRUE;
    }

/*
    end path
*/

    if (PR_UNSUCCESSFUL(ps = PR_parseTokenSequence("KK", Ks.end, Ks.path))) {
        PR_ePaths();
        return;
    }
    if (! foundStatement) {
        t = &(ps -> token[1]);
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_ERROR + PR_PATH_ERROR + PR_MISSING_BODY));
    }

    return;

pathFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_PATH_ERROR + errorCode));

pathError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_PATH_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.path);
    PR_dPaths();
}

static void PR_dPaths(void)
{
    struct PathData *p;
    int             i;

    if (PathData.next == (struct PathData *) 0)
        return;
    for (p = &PathData;
         p -> next -> next != (struct PathData *) 0;
         p = p -> next)
        ;

    if (p -> next -> pSymbolName != (DMI_STRING *) 0)
        OS_Free(p -> next -> pSymbolName);
    for (i = 1; i <= BIF_LAST_OS; ++i)
        if (p -> next -> environmentData[i].pPathName != (DMI_STRING *) 0)
            OS_Free(p -> next -> environmentData[i].pPathName);
    OS_Free(p -> next);
    p -> next = (struct PathData *) 0;
}

static void PR_ePaths(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseIdent(0))) {
        if (PR_SUCCESSFUL(ps = PR_parseEquals())) {
            t = PR_tokenTableGetCurrent();
            errorCode = PR_EXPECTING_LITERAL;
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_EQUALS;
        }
    } else if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.end))) {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_END;
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_STATEMENT;
    }
    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_PATH_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.path);
}

unsigned long PR_pathCount(void)
{
    unsigned long   count;
    struct PathData *p;

    count = 0;
    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        ++count;

    return count;
}

unsigned long PR_pathEnvironmentCount(unsigned long pathIndex)
{
    struct PathData *p;
    unsigned long   environmentCount;
    int             i;

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (--pathIndex == 0) {
            environmentCount = 0;
            for (i = 1; i <= BIF_LAST_OS; ++i)
                if (p -> environmentData[i].iEnvironmentId != 0)
                    ++environmentCount;
            return environmentCount;
        }
    return 0;
}

unsigned long PR_pathEnvironmentId(unsigned long pathIndex,
                                   unsigned long environmentIndex)
{
    struct PathData *p;
    int             i;

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (--pathIndex == 0) {
            for (i = 1; i <= BIF_LAST_OS; ++i)
                if (p -> environmentData[i].iEnvironmentId != 0)
                    if (--environmentIndex == 0)
                        return  p ->
                            environmentData[i].iEnvironmentId;
            return 0;
        }
    return 0;
}

DMI_STRING *PR_pathEnvironmentPathName(unsigned long pathIndex,
                                         unsigned long environmentIndex)
{
    struct PathData *p;
    int             i;

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (--pathIndex == 0) {
            for (i = 1; i <= BIF_LAST_OS; ++i)
                if (p -> environmentData[i].iEnvironmentId != 0)
                    if (--environmentIndex == 0)
                        return  p -> environmentData[i].pPathName;
            return (DMI_STRING *) 0;
        }
    return (DMI_STRING *) 0;
}

MIF_Bool_t PR_pathExists(DMI_STRING *symbolicName, DMI_STRING *environment)
{
    struct PathData *p;

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (PR_stringCmp(p -> pSymbolName, symbolicName) == 0)
            if (environment != (DMI_STRING *) 0)
                return p -> environmentData[BIF_stringToEnvironmentId(
                               environment)].iEnvironmentId != 0;
            else
                return MIF_TRUE;

    return MIF_FALSE;
}

DMI_STRING *PR_pathSymbolName(unsigned long pathIndex)
{
    struct PathData *p;

    for (p = PathData.next; p != (struct PathData *) 0; p = p -> next)
        if (--pathIndex == 0)
            return p -> pSymbolName;
    return (DMI_STRING *) 0;
}

DMI_STRING *PR_pathTargetOs(void)
{
    static PR_uString(string, 80);

    PR_stringInit((DMI_STRING *) string, PR_TARGET_OS);
    return (DMI_STRING *) string;
}
