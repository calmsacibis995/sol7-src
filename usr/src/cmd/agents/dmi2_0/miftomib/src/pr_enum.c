/* Copyright 10/17/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_enum.c	1.5 96/10/17 Sun Microsystems"


#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)pr_enum.c	1.5 96/10/17 Sun Microsystems"
#else
static char sccsid[] = "@(#)pr_enum.c	1.5 96/10/17 Sun Microsystems";
#endif
#endif

/**********************************************************************
    Filename: pr_enum.c
    
    Copyright (C) International Business Machines, Corp. 1995
    Copyright (c) Intel, Inc. 1992,1993,1994

    Description: MIF Parser Enumeration Routines

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_enum.c 1.18 1994/09/02 19:56:55 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Use new identification parsing.
                        Added octal and hexadecimal support.
        7/28/93  aip    Use new dmi.h
        8/19/93  aip    Enumeration values can be negative.
        8/26/93  aip    Modified pEnumeration such that enumeration names in
                        the MIF database are case insensitive.
        11/11/93 sfh	Include os_dmi.h instead of dos_dmi.h.
        12/3/93  aip    Change enumeration "constants" from identifiers to
                        literals.  Reversed ordering of enumeration statement.
        12/22/93 aip    Enumeration names made case sensitive.
        2/28/94  aip    BIF
        4/7/94   aip    32-bit strings.
        6/10/94  aip    Fixed enumeration problem.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/5/94   aip    Keep enumeration display strings in the token table.
        8/29/94  aip    ANSI-compliant.
        10/28/94 aip    Use handles.
        10/10/95 par    Modified to remove dead code

************************* INCLUDES ***********************************/

#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "os_dmi.h"
#include "mif_db.h"
#include "pr_err.h"
#include "pr_enum.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_tok.h"
#include "pr_todmi.h"
#include "os_svc.h"
#include "common.h"		/* For definition of MIF_INTEGER */

/*********************************************************************/

/************************ PRIVATE ************************************/

static void PR_cEnum(PR_Token_t *t);
static void PR_dEnum(void);
static void PR_eEnum(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

struct ElementData {
    struct ElementData *next;
    MIF_Pos_t          elementNamePos;
    long               iValue;
    unsigned long      osString;
};                  

static struct EnumerationData {
    struct EnumerationData *next;
    DMI_STRING             *pEnumerationName;
    void                   *pGroupHandle;
    unsigned long          iAttributeId;
    struct ElementData     elementData;
} EnumerationData;

/*********************************************************************/

void PR_enumerationInit(void)
{
    struct EnumerationData *en;
    struct ElementData     *el;

    while (EnumerationData.next != (struct EnumerationData *) 0) {
        en = EnumerationData.next;
        if (en -> pEnumerationName != (DMI_STRING *) 0)
            OS_Free(en -> pEnumerationName);
        while (en -> elementData.next != (struct ElementData *) 0) {
            el = en -> elementData.next;
            en -> elementData.next = el -> next;
            OS_Free(el);
        }
        EnumerationData.next = en -> next;
        OS_Free(en);
    }
    memset(&EnumerationData,0,sizeof(struct EnumerationData));
}

void PR_pEnumeration(PR_Token_t *t, void *groupHandle,
                     unsigned long attributeId)
{
    PR_ErrorNumber_t        errorCode;
    PR_ParseStruct_t        *ps;
    char                    *literal;
    char                    name[PR_LITERAL_MAX + 1];
/*
    PR_uString              (string, PR_LITERAL_MAX);
*/
#pragma align 2 (string)
    static char string[(PR_LITERAL_MAX)+sizeof(unsigned long)];
	
    unsigned long           stringSize;
    BIF_AttributeDataType_t dataType;
    MIF_Unsigned_t          value;
    PR_Token_t              *elementNameToken;
    struct EnumerationData  *e;
    struct ElementData      *el;

    stringSize = 0;

    Scope = PR_ENUM_SCOPE;

/*
    name = <literal>
*/

    if (PR_UNSUCCESSFUL(
        ps = PR_pIdentification(attributeId ? "l" : "L", Ks.name)))
        return;
    t = &(ps -> token[0]);
    if (t -> tokenNumber != 0) {
        strcpy(name, PR_tokenLiteralGet(t));
        PR_stringInit((DMI_STRING *) string, name);
        stringSize = PR_stringSize((DMI_STRING *) string);
    }

/*
    If name is not anonymous then it should be unique
*/

    if (attributeId == 0)
        for (e = EnumerationData.next;
             e != (struct EnumerationData *) 0;
             e = e -> next)
            if ((e -> iAttributeId == 0) &&
                PR_stringCmp(e -> pEnumerationName,
                             (DMI_STRING *) string) == 0) {
                errorCode = PR_CONFLICTING;
                goto enumerationError;
            }

/*
    Add name to EnumerationData array
*/

    for (e = &EnumerationData;
         e -> next != (struct EnumerationData *) 0;
         e = e -> next)
        ;

    if ((e -> next =
         (struct EnumerationData *) OS_Alloc(sizeof(struct EnumerationData))) ==
        (struct EnumerationData *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto enumerationFatal;
    }
    e = e -> next;
    e -> next = (struct EnumerationData *) 0;
    e -> pGroupHandle = groupHandle;
    e -> iAttributeId = attributeId;
    e -> elementData.next = (struct ElementData *) 0;

    if (attributeId == 0) {
        if ((e -> pEnumerationName = (DMI_STRING *)
                                     OS_Alloc((size_t) stringSize)) ==
            (DMI_STRING *) 0) {
            errorCode = PR_OUT_OF_MEMORY;
            goto enumerationFatal;
        }
        memcpy(e -> pEnumerationName, string, (size_t) stringSize);
    } else
        e -> pEnumerationName = (DMI_STRING *) 0;

/*
    [type = <ident>]
*/

    if (PR_SUCCESSFUL(PR_parseKeyword(Ks.type)))
        if (PR_SUCCESSFUL(ps = PR_parseEquals())) {
            ps = PR_parseIdent((char *) 0);
            t = &(ps -> token[0]);
            if (PR_SUCCESSFUL(ps)) {
                literal = PR_tokenLiteralGet(t);
                PR_stringInit((DMI_STRING *) string, literal);
                dataType = BIF_stringToType((DMI_STRING *) string);
                if (dataType == MIF_UNKNOWN_TYPE) {
                    errorCode = PR_ILLEGAL_VALUE;
                    goto enumerationError;
                }
                if (dataType != MIF_INTEGER) {
                    errorCode = PR_NOT_IMPLEMENTED;
                    goto enumerationError;
                }
            } else {
                t = &(ps -> token[0]);
                errorCode = PR_ILLEGAL_VALUE;
                goto enumerationError;
            }
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_EQUALS;
            goto enumerationError;
        }
    else
        dataType = BIF_INT;

    for (;;) {

/*
        <value> = <literal>
*/

        if (PR_SUCCESSFUL(
            ps = PR_parseTokenSequence("cNCL", '-', (char *) 0, '=',
                                       (char *) 0))) {
            if (ps -> tokenCount == 4) {
                value = -1 * PR_atol(PR_tokenLiteralGet(&(ps -> token[1])));
                elementNameToken = &(ps -> token[3]);
            } else {
                value = PR_atol(PR_tokenLiteralGet(&(ps -> token[0])));
                elementNameToken = &(ps -> token[2]);
            }
            t = &(ps -> token[0]);

/*
            Value should be unique within an enumeration
*/

            for (el = e -> elementData.next;
                 el != (struct ElementData *) 0;
                 el = el -> next)
                if (el -> iValue == (MIF_Int_t) value) {
                    errorCode = PR_CONFLICTING;
                    goto enumerationError;
                }

/*
            Add value and symbol to enumeration
*/

            for (el = &e -> elementData;
                 el -> next != (struct ElementData *) 0;
                 el = el -> next)
                ;
            if ((el -> next =
                 (struct ElementData *) OS_Alloc(sizeof(struct ElementData))) ==
                (struct ElementData *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto enumerationFatal;
            }
            el = el -> next;
            el -> next = (struct ElementData *) 0;
            el -> elementNamePos.page = elementNameToken -> value.valuePos.page;
            el -> elementNamePos.offset = elementNameToken ->
                                          value.valuePos.offset;
            el -> iValue = value;

/*
        End Enumeration
*/

        } else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.end, Ks.enumeration))) {
            PR_cEnum(&(ps -> token[1]));
            return;
        } else {
            PR_eEnum();
            return;
        }
    }

enumerationFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_ENUM_ERROR + errorCode));

enumerationError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ENUM_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.enumeration);
    PR_dEnum();
}

static void PR_cEnum(PR_Token_t *t)
{
    struct EnumerationData *e;

    for (e = EnumerationData.next; e -> next != 0; e = e -> next)
        ;
    if (e == (struct EnumerationData *) 0)
        return;

    if (e -> elementData.next == (struct ElementData *) 0) {
        PR_errorLogAdd(t -> line, t -> col, PR_ERROR + PR_ENUM_ERROR +
                       PR_MISSING_BODY); 
       PR_dEnum();
    }
}

static void PR_dEnum(void)
{
    struct EnumerationData *e;
    struct ElementData     *el;

    if (EnumerationData.next == 0)
        return;
    for (e = &EnumerationData;
         e -> next -> next != (struct EnumerationData *) 0;
         e = e -> next)
        ;

    if (e -> next -> pEnumerationName != (DMI_STRING *) 0)
        OS_Free(e -> next -> pEnumerationName);

    while (e -> elementData.next != (struct ElementData *) 0) {
        for (el = &e -> elementData; el -> next -> next != 0; el = el -> next)
           ;
        OS_Free(el -> next);
        el -> next = (struct ElementData *) 0;
    }
    OS_Free(e -> next);
    e -> next = (struct EnumerationData *) 0;
}

static void PR_eEnum(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseNumber((char *) 0))) {
        if (PR_SUCCESSFUL(ps = PR_parseEquals())) {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_LITERAL;
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_EQUALS;
        }
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_VALUE;
    }
    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ENUM_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.enumeration);
    PR_dEnum();
}

unsigned long PR_enumElementCount(void *enumHandle)
{
    struct EnumerationData *e;
    struct ElementData     *el;
    unsigned long          elementCount;

    if (enumHandle == (void *) 0)
        return 0;

    e = (struct EnumerationData *) enumHandle;
    elementCount = 0;
    for (el = e -> elementData.next;
         el != (struct ElementData *) 0;
         el = el -> next)
        ++elementCount;

    return elementCount;
}

MIF_Bool_t PR_enumElementExists(void *enumHandle, DMI_STRING *elementName)
{
    struct EnumerationData *e;
    struct ElementData     *el;

    if (enumHandle == (void *) 0)
        return MIF_FALSE;

    e = (struct EnumerationData *) enumHandle;
    for (el = e -> elementData.next;
         el != (struct ElementData *) 0;
         el = el -> next)
        if (PR_stringCmp(PR_tokenTableStringGet(el -> elementNamePos),
                         elementName) == 0)
            return MIF_TRUE;

    return MIF_FALSE;
}

DMI_STRING *PR_enumElementIndexToName(void *enumHandle,
                                      unsigned long elementIndex)
{
    struct EnumerationData *e;
    struct ElementData     *el;
    unsigned long          i;

    if (enumHandle == (void *) 0)
        return (DMI_STRING *) 0;

    e = (struct EnumerationData *) enumHandle;
    for (el = e -> elementData.next, i = 1;
         (el != (struct ElementData *) 0) && (i < elementIndex);
         el = el -> next, ++i)
        ;

    if (el == (struct ElementData *) 0)
        return (DMI_STRING *) 0;

    return PR_tokenTableStringGet(el -> elementNamePos);
}

MIF_Int_t PR_enumElementValue(void *enumHandle, DMI_STRING *elementName)
{
    struct EnumerationData *e;
    struct ElementData     *el;

    if (enumHandle == (void *) 0)
        return 0;

    e = (struct EnumerationData *) enumHandle;
    for (el = e -> elementData.next;
         el != (struct ElementData *) 0;
         el = el -> next)
        if (PR_stringCmp(PR_tokenTableStringGet(el -> elementNamePos),
                         elementName) == 0)
            return el -> iValue;

    return 0;
}

DMI_STRING *PR_enumHandleToName(void *enumHandle)
{
    if (enumHandle == (void *) 0)
        return (DMI_STRING *) 0;

    return ((struct EnumerationData *) enumHandle) -> pEnumerationName;
}

void *PR_enumIdToHandle(void *groupHandle, unsigned long attributeId)
{
    struct EnumerationData *e;

    for (e = EnumerationData.next;
         e != (struct EnumerationData *) 0;
         e = e -> next)
        if  ((e -> pEnumerationName == (DMI_STRING *) 0) &&
             (e -> pGroupHandle == groupHandle) &&
             (e -> iAttributeId == attributeId))
            return (void *) e;
 
    return (void *) 0;
}

void *PR_enumNameToHandle(DMI_STRING *enumName)
{
    struct EnumerationData *e;

    for (e = EnumerationData.next;
         e != (struct EnumerationData *) 0;
         e = e -> next)
        if (((e -> pEnumerationName != (DMI_STRING *) 0) &&
             (PR_stringCmp(e -> pEnumerationName, enumName) == 0)))
            return (void *) e;
 
    return (void *) 0;
}
