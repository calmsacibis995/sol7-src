/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_comp.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_comp.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser component routines

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_comp.c 1.13 1994/08/29 15:46:41 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Use new identification parsing.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit strings.
        6/10/94  aip    Fixed enumeration problem.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/4/94   aip    Keep descriptions in token table.
        8/29/94  aip    Zero component structure in initialization routine.
                        ANSI-compliant.
        10/28/94 aip    Use group handles.
        10/10/95 par    Modified to remove dead code.

************************* INCLUDES ***********************************/

#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "mif_db.h"
#include "pr_comp.h"
#include "pr_enum.h"
#include "pr_group.h"
#include "pr_parse.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_path.h"
#include "pr_plib.h"
#include "pr_table.h"
#include "os_svc.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static void PR_cComponent(PR_Token_t *t);
static void PR_eComponent(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

static struct ComponentData {
    DMI_STRING         *pComponentName;
    MIF_Pos_t          descriptionPos;
} ComponentData;

/*********************************************************************/

void PR_componentInit(void)
{
    if (ComponentData.pComponentName != (DMI_STRING *) 0)
        OS_Free(ComponentData.pComponentName);
    memset(&ComponentData, 0, sizeof(struct ComponentData));
}

void PR_pComponent(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;
    char             *literal;
    PR_uString       (string, PR_LITERAL_MAX);
    unsigned long    stringSize;

    Scope = PR_COMPONENT_SCOPE;

/*
    Name = <literal>
*/

    if (PR_UNSUCCESSFUL(
        ps = PR_pIdentification("L", Ks.name)))
        return;
    t = &(ps -> token[0]);
    literal = PR_tokenLiteralGet(t);
    PR_stringInit((DMI_STRING *) string, literal);
    stringSize = PR_stringSize((DMI_STRING *) string);
    if ((ComponentData.pComponentName = (DMI_STRING *)
                                        OS_Alloc((size_t) stringSize)) ==
        (DMI_STRING *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto componentFatal;
    }
    memcpy(ComponentData.pComponentName, string, (size_t) stringSize);

    for (;;) {
        Scope = PR_COMPONENT_SCOPE;

/*
        Start Enumeration
*/

        if (PR_SUCCESSFUL(
                ps = PR_parseTokenSequence("KK", Ks.start, Ks.enumeration)))
            PR_pEnumeration(&(ps -> token[1]), (void *) 0, 0);

/*
        Start Paths
*/

        else if (PR_SUCCESSFUL(
                ps = PR_parseTokenSequence("KK", Ks.start, Ks.path))) 
            PR_pPaths();

/*
        Start Group
*/

        else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.start, Ks.group)))
            PR_pGroup();

/*
        Start Table
*/

        else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.start, Ks.table)))
            PR_pTable();

/*
        Description <literal>
*/

        else if (PR_SUCCESSFUL(PR_pDescription(&ComponentData.descriptionPos)))
            ;

/*
        End Component
*/

        else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.end, Ks.component))) {
            t = &(ps -> token[1]);
            PR_cComponent(t);
            return;
        } else {
            PR_eComponent();
            return;
        }
    }

componentFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_COMPONENT_ERROR + errorCode));
}

static void PR_cComponent(PR_Token_t *t)
{
    PR_ErrorNumber_t errorCode;

    if (PR_groupCount() == 0) {
        errorCode = PR_ERROR + PR_NO_GROUPS;
        goto componentError;
    }
    if (PR_unusedTemplateCount() > 0) {
        errorCode = PR_INFO + PR_UNUSED_TEMPLATES;
        goto componentError;
    }
    if (! PR_groupExists(PR_COMPONENT_ID_GROUP)) {
        errorCode = PR_WARN + PR_CID_ERROR;
        goto componentError;
    }
    return;

componentError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_COMPONENT_ERROR + errorCode));
}

static void PR_eComponent(void)
{
    PR_ErrorNumber_t errorCode;
    PR_Token_t       *t;

    t = PR_tokenTableGetCurrent();
    if (PR_SUCCESSFUL(PR_parseKeyword(Ks.start))) {
        t = PR_tokenTableGetCurrent();
        errorCode = PR_EXPECTING_BLOCK;
    } else
        errorCode = PR_EXPECTING_STATEMENT;

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_COMPONENT_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.component);
}

DMI_STRING *PR_componentDescription(void)
{
     return PR_tokenTableStringGet(ComponentData.descriptionPos);
}

DMI_STRING *PR_componentName(void)
{
    return ComponentData.pComponentName;
}
