/* Copyright 10/01/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_attr.c	1.3 96/10/01 Sun Microsystems"


/**********************************************************************
    Filename: pr_attr.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF parser attribute routines

    Author(s): Alvin I. Pivowar
               Paul A. Ruocchio

    RCS Revision: $Header: j:/mif/parser/rcs/pr_attr.c 1.36 1994/09/14 13:28:24 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added Storage = statement
                        Use new Identification parsing
                        Added Write-Only support
        7/28/93  aip    Changed mask names to MIF_...
                        Use new dmi.h
        8/4/93   aip    Modified cAttribute to create data object for
                        write-only attributes.
        8/19/93  aip    Changed "OctetString" to "Octetstring"
        8/26/93  aip    Fixed error handlig in PR_pAccessStmt.
        9/9/93   aip    Fixed mispelling of Counter64.
        11/2/93  aip    Fixed bug that allowed null literal values for integers.
                        Use new MIF_dataValueGet()
        11/10/93 aip    Fixed bug introduced 11/2/93 that disallowed Value
                        statements for numeric types other than MIF_INTEGER.
        11/11/93 sfh	Include os_dmi.h instead of dos_dmi.h.
        12/3/93  aip    Changed enumeration "constants" from identifiers to
                        literals.
        12/6/93  aip    New path overlay syntax.
        12/22/93 aip    Enumerations are literals.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit string.
                        4.4
        4/21/94  aip    Added date and 64-bit types.
        5/20/94  aip    Removed info message regarding counter64.
        6/1/94   aip    Suppress missing value error with unsupported
                        attributes.
        6/7/94   aip    Fixed illegal value error with negative integers.
        6/10/94  aip    Fixed string data size problem.
                        Fixed enumeration problem.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        7/22/94  aip    Suppress missing instrumentation message.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/4/94   aip    The maxSize of string attributes needs to account for
                        the space for the leading length.
                        Keep descriptions in token table.
        8/5/94   aip    Allow null-string values for dates.
        9/8/94   par    If the Installation data/time is NULL at install
                        time, the parser sets it to the current time.
                        Date type verification routine handes the '-'
                        separator type correctly.
        8/29/94  aip    ANSI-compliant.
        9/2/94   aip    Moved PR_cDate() from private to public.
        9/14/94  aip    Fill in null values for the installation time in the
                        component ID group with the local time.
        10/28/94 aip    Fixed write-only bug.
                        Fixed storage statement error handling.
                        Use group handles.
        10/10/95 par    Modified to remove dead code.

************************* INCLUDES ***********************************/

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "mif_db.h"
#include "pr_attr.h"
#include "pr_enum.h"
#include "pr_group.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_path.h"
#include "pr_todmi.h"
#include "os_svc.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static void       PR_cAttribute(void *groupHandle, PR_Token_t *t);
static void       PR_dAttribute(void);
static void       PR_eAttribute(void);
static void       PR_pAccessStmt(void);
static void       PR_pStorageStmt(void);
static void       PR_pTypeStmt(void);
static void       PR_pValueStmt(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

static struct AttributeData {
    struct AttributeData *next;
    DMI_STRING           *pAttributeName;
    MIF_Pos_t            descriptionPos;
    void                 *pEnumerationHandle;
    void                 *pValue;
    unsigned long        osAttributeName;
    unsigned long        osDescription;
    unsigned long        iAccess;
    unsigned long        iStorage;
    unsigned long        iType;
    unsigned long        iEnumCount;
    unsigned long        osEnumList;
    unsigned long        iMaxSize;
    unsigned long        iAttributeId;
    unsigned long        iValueType;
    unsigned long        osValue;
    void                 *pGroupHandle;
} AttributeData;

/*********************************************************************/

void PR_attributeInit(void)
{
    struct AttributeData *a;

    while (AttributeData.next != (struct AttributeData *) 0) {
        a = AttributeData.next;
        if (a -> pAttributeName != (DMI_STRING *) 0)
            OS_Free(a -> pAttributeName);
        if (a -> pValue != (void *) 0)
            OS_Free(a -> pValue);
        AttributeData.next = a -> next;
        OS_Free(a);
    }
    memset(&AttributeData,0,sizeof(struct AttributeData));
}

void PR_pAttribute(void *groupHandle)
{
    PR_ErrorNumber_t     errorCode;
    PR_ParseStruct_t     *ps;
    PR_Token_t           *t;
    char                 *literal;
    PR_uString           (string, PR_LITERAL_MAX);
    unsigned long        stringSize;
    struct AttributeData *a;
    unsigned long        attributeId;
    size_t               i;

    t = (PR_Token_t *) 0;
    stringSize = 0;
    attributeId = 0;

    Scope = PR_ATTRIBUTE_SCOPE;
    a = (struct AttributeData *) 0;

/*
    name = <literal>
    id = <integer>
*/

    if (PR_UNSUCCESSFUL(ps = PR_pIdentification("IL", Ks.id, Ks.name)))
        return;

    for (i = 0; i < strlen("IL"); ++i) {
        t = &(ps -> token[i]);
        literal = PR_tokenLiteralGet(t);
        switch (t -> type) {
            case PR_TOK_NUMBER:
                attributeId = PR_atol(literal);
                break;
            case PR_TOK_LITERAL:
                PR_stringInit((DMI_STRING *) string, literal);
                stringSize = PR_stringSize((DMI_STRING *) string);
                break;
        }
    }

/*
    Check for conflicting ID
*/

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId)) {
            errorCode = PR_CONFLICTING;
            goto attributeError;
        }

/*
    Add new attribute to list
*/

    for (a = &AttributeData;
         a -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;
    if ((a -> next =
         (struct AttributeData *) OS_Alloc(sizeof(struct AttributeData))) ==
        (struct AttributeData *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto attributeFatal;
    }
    a = a -> next;
    a -> next = (struct AttributeData *) 0;
    if ((a -> pAttributeName = (DMI_STRING *) OS_Alloc((size_t) stringSize)) ==
        (DMI_STRING *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto attributeFatal;
    }
    memcpy(a -> pAttributeName, string, (size_t) stringSize);
    a -> descriptionPos.page = 0;
    a -> descriptionPos.offset = 0;
    a -> pEnumerationHandle = (void *) 0;
    a -> pValue = (void *) 0;
    a -> iAccess = BIF_UNKNOWN_ACCESS_TYPE;  
    a -> iStorage = (unsigned long) BIF_UNKNOWN_STORAGE_TYPE;
    a -> iType = BIF_UNKNOWN_DATA_TYPE;
    a -> iMaxSize = 0;
    a -> iAttributeId = attributeId;
    a -> iValueType = BIF_VALUE_TYPE;
    a -> pGroupHandle = groupHandle;

    for (;;) {

/*
        access = <ident>
*/

        if (PR_SUCCESSFUL(PR_parseKeyword(Ks.access)))
            PR_pAccessStmt();


/*
        Storage = Common
        Storage = Specific
*/

        else if (PR_SUCCESSFUL(PR_parseKeyword(Ks.storage)))
            PR_pStorageStmt();

/*
        type = <ident>
        type = <literal>
*/

        else if (PR_SUCCESSFUL(PR_parseKeyword(Ks.type)))
            PR_pTypeStmt();

/*
        value = <value>
*/

        else if (PR_SUCCESSFUL(PR_parseKeyword(Ks.value)))
            PR_pValueStmt();

/*
        Description <literal>
*/

        else if (PR_SUCCESSFUL(PR_pDescription(&a -> descriptionPos)))
            ;

/*
        End Attribute
*/

        else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.end, Ks.attribute))) {
            PR_cAttribute(groupHandle, &(ps -> token[1]));
            return;
        } else {
            PR_eAttribute();
            return;
        }
    }

attributeFatal:

    if(t != (PR_Token_t *)NULL)
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_FATAL + PR_ATTRIBUTE_ERROR + errorCode));
    else
        PR_errorLogAdd(0,0,(PR_ErrorNumber_t) (PR_FATAL + PR_ATTRIBUTE_ERROR + errorCode));


attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.attribute);
    if (a != (struct AttributeData *) 0)
        PR_dAttribute();
}

static void PR_cAttribute(void *groupHandle, PR_Token_t *t)
{
    PR_ErrorNumber_t     errorCode;
    struct AttributeData *a;

    for (a = AttributeData.next;
         a -> next != (struct AttributeData *) 0;
         a = a-> next)
        ;
    if (a == (struct AttributeData *) 0)
        return;

    if (a -> iStorage == BIF_UNKNOWN_STORAGE_TYPE)
        a -> iStorage = BIF_SPECIFIC_STORAGE;
    if (a -> iAccess == BIF_UNKNOWN_ACCESS_TYPE)
        a -> iAccess = BIF_READ_ONLY;
    if (a -> iType == BIF_UNKNOWN_DATA_TYPE) {
        errorCode = PR_MISSING_TYPE;
        goto attributeError;
    }
    if (a -> iMaxSize == 0) {
        errorCode = PR_MISSING_SIZE;
        goto attributeError;
    }
/* wy10-1 unknown or no value is ok
    if (a -> pValue == (void *) 0)
        if ((PR_groupId(groupHandle) != 0) &&
            (a -> iAccess != BIF_UNSUPPORTED) &&
            (a->iAccess != BIF_UNKNOWN) &&
            (a -> iAccess != BIF_WRITE_ONLY)) {
            errorCode = PR_MISSING_VALUE;
            goto attributeError;
        }
*/
    return;

attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
    PR_dAttribute();
}

static void PR_dAttribute(void)
{
    struct AttributeData *a;

    if (AttributeData.next == (struct AttributeData *) 0)
        return;

    for (a = &AttributeData;
         a -> next -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;

    if (a -> next -> pAttributeName != (DMI_STRING *) 0)
        OS_Free(a -> next -> pAttributeName);
    if (a -> next -> pValue != (void *) 0)
        OS_Free(a -> next -> pValue);
    OS_Free(a -> next);
    a -> next = (struct AttributeData *) 0;
}

static void PR_eAttribute(void)
{
    PR_Token_t       *t;

    t = PR_tokenTableGetCurrent();
    PR_errorLogAdd(t -> line, t -> col, PR_ERROR + PR_ATTRIBUTE_ERROR +
                   PR_EXPECTING_STATEMENT);
    PR_skipToTokenSequence("KK", Ks.end, Ks.attribute);
    PR_dAttribute();
}

static void PR_pAccessStmt(void)
{
    PR_ErrorNumber_t      errorCode;
    PR_ParseStruct_t      *ps;
    PR_Token_t            *t;
    char                  *literal;
    struct AttributeData  *a;
    BIF_AttributeAccess_t access;

    for (a = AttributeData.next;
         a -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;

/*
    =
*/

    if (PR_SUCCESSFUL(ps = PR_parseEquals())) {

/*
        Read-Only
        Read-Write
        Write-Only
*/

        if (PR_SUCCESSFUL(ps = PR_parseIdent("Read-only")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Read-write")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Write-only"))) {
            t = &(ps -> token[0]);
            literal = PR_tokenLiteralGet(t);
            if (strcmp(literal, "Read-only") == 0)
               access= BIF_READ_ONLY;
            else if (strcmp(literal, "Read-write") == 0)
                access = BIF_READ_WRITE;
            else
                access = BIF_WRITE_ONLY;
            if (a -> iAccess != BIF_UNKNOWN_ACCESS_TYPE) {
                if (access == (BIF_AttributeAccess_t) a -> iAccess)
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            } else
                a -> iAccess = access;
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_ILLEGAL_VALUE;
            goto attributeError;
        }
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_EQUALS;
        goto attributeError;
    }

    if ((access == BIF_WRITE_ONLY) && (a -> pValue != (void *) 0)) {
        errorCode = PR_WRITE_ONLY;
        goto attributeError;
    }

    return;

attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
}

static void PR_pStorageStmt(void)
{
    PR_ErrorNumber_t     errorCode;
    PR_ParseStruct_t     *ps;
    PR_Token_t           *t;
    struct AttributeData *a;

    t = (PR_Token_t *) 0;

    for (a = AttributeData.next;
         a -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;

/*
    =
*/

    if (PR_SUCCESSFUL(ps = PR_parseEquals()))

/*
        Common
*/

        if (PR_SUCCESSFUL(ps = PR_parseIdent("Common"))) {
            if (a -> iStorage !=
                BIF_UNKNOWN_STORAGE_TYPE) {
                t = &(ps -> token[0]);
                if (a -> iStorage == BIF_COMMON_STORAGE)
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            }
            a -> iStorage = BIF_COMMON_STORAGE;

/*
        Specific
*/

        } else if (PR_SUCCESSFUL(ps = PR_parseIdent("Specific"))) {
            if (a -> iStorage !=
                BIF_UNKNOWN_STORAGE_TYPE) {
                t = &(ps -> token[0]);
                if (a -> iStorage == BIF_SPECIFIC_STORAGE)
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            }
            a -> iStorage = BIF_SPECIFIC_STORAGE;
        } else {
            t = PR_tokenTableGetCurrent();
            errorCode = PR_ILLEGAL_VALUE;
            goto attributeError;
        }
    else {
        t = PR_tokenTableGetCurrent();
        errorCode = PR_EXPECTING_EQUALS;
        goto attributeError;
    }
    
    return;

attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
}

static void PR_pTypeStmt(void)
{
    PR_ErrorNumber_t        errorCode;
    PR_ParseStruct_t        *ps;
    PR_Token_t              *t;
    char                    *literal;
    PR_uString              (string, PR_LITERAL_MAX);
    unsigned long           stringSize;
    struct AttributeData    *a;
    BIF_AttributeDataType_t type;

    type = BIF_UNKNOWN_DATA_TYPE;

    for (a = AttributeData.next;
         a -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;

/*
    =
*/

    if (PR_SUCCESSFUL(ps = PR_parseEquals())) {

/*
    Counter
    Counter32
    Counter64
    Gauge
    Octetstring [<integer>]
    Displaystring [<integer>]
    String [<integer>]
    Integer
    Int
    Integer64
*/

        if (PR_SUCCESSFUL(ps = PR_parseIdent("Counter")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Counter64")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Date")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Gauge")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Octetstring")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Displaystring")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("String")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Integer")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Int")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Integer64")) ||
            PR_SUCCESSFUL(ps = PR_parseIdent("Int64"))) {
            t = &(ps -> token[0]);
            literal = PR_tokenLiteralGet(t);
            if (strcmp(literal, "Counter") == 0)
                type = BIF_COUNTER;
            else if (strcmp(literal, "Counter64") == 0)
                type = BIF_COUNTER64;
            else if (strcmp(literal, "Date") == 0)
                type = BIF_DATE;
            else if (strcmp(literal, "Gauge") == 0)
                type = BIF_GAUGE;
            else if (strcmp(literal, "Octetstring") == 0)
                type = BIF_OCTETSTRING;
            else if (strcmp(literal, "Displaystring") == 0)
                type = BIF_DISPLAYSTRING;
            else if (strcmp(literal, "String") == 0)
                type = BIF_DISPLAYSTRING;
            else if (strcmp(literal, "Integer") == 0)
                type = BIF_INT;
            else if (strcmp(literal, "Int") == 0)
                type = BIF_INT;
            else if (strcmp(literal, "Integer64") == 0)
                type = BIF_INT64;
            else if (strcmp(literal, "Int64") == 0)
                type = BIF_INT64;
            if (a -> iType != BIF_UNKNOWN_DATA_TYPE) {
                if (type == (BIF_AttributeDataType_t) a -> iType)
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            } else
                a -> iType = type;
            if (strstr(literal, "tring") != (char *) 0) {
                if (PR_SUCCESSFUL(PR_parseLeftParen()))
                    if (PR_SUCCESSFUL(
                            ps = PR_parseTokenSequence("N", (char *) 0))) {
                        a -> iMaxSize = sizeof(unsigned long) +
                            PR_atol(PR_tokenLiteralGet(&(ps -> token[0])));
                        if (a -> iMaxSize == 0) {
                            t = &(ps -> token[0]);
                            errorCode = PR_ILLEGAL_VALUE;
                            goto attributeError;
                        }
                        if (PR_UNSUCCESSFUL(ps = PR_parseRightParen())) {
                            t = &(ps -> token[0]);
                            errorCode = PR_EXPECTING_RIGHT_PAREN;
                            goto attributeError;
                        }
                    } else {
                        t = &(ps -> token[0]);
                        errorCode = PR_EXPECTING_INTEGER;
                        goto attributeError;
                    }
            } else
                switch (type) {
                case BIF_DATE:
                    a -> iMaxSize = sizeof(MIF_Date_t);
                    break;
                case BIF_COUNTER64:  /* FALL THROUGH */
                case BIF_INT64:
                    a -> iMaxSize = sizeof(PR_Int64_t);
                    break;
                default:
                    a -> iMaxSize = sizeof(MIF_Int_t);
                    break;
                }

            return;

/*
        Type = <literal>
*/ 

        } else if (PR_SUCCESSFUL(ps = PR_parseLiteral((char *) 0))) {
            t = &(ps -> token[0]);
            literal = PR_tokenLiteralGet(t);
            PR_stringInit((DMI_STRING *) string, literal);
            stringSize = PR_stringSize((DMI_STRING *) string);
      
            if (a -> iType != BIF_UNKNOWN_DATA_TYPE) {
                if ((a -> pEnumerationHandle != (void *) 0) &&
                    (PR_stringCmp((DMI_STRING *) string,
                    PR_enumHandleToName(a -> pEnumerationHandle)) == 0))
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            }

            a -> pEnumerationHandle =
                PR_enumNameToHandle((DMI_STRING *) string);
            if (a -> pEnumerationHandle == (void *) 0) {
                errorCode = PR_ILLEGAL_VALUE;
                goto attributeError;
            }
            a -> iType = BIF_INT;
            a -> iMaxSize = sizeof(MIF_Int_t);

            return;

/*
        Type = <anonymous enumeration declaration>
*/

        } else if (PR_SUCCESSFUL(
                  ps = PR_parseTokenSequence("KK", Ks.start, Ks.enumeration))) {
            t = &(ps -> token[1]);
            PR_pEnumeration(t, a -> pGroupHandle,
                            a -> iAttributeId);
            Scope = PR_ATTRIBUTE_SCOPE;
            if (a -> iType != BIF_UNKNOWN_DATA_TYPE) {
                if (a -> pEnumerationHandle ==
                    PR_enumIdToHandle(a -> pGroupHandle,
                                        a -> iAttributeId))
                    errorCode = PR_DUPLICATE_STATEMENT;
                else
                    errorCode = PR_CONFLICTING;
                goto attributeError;
            }

            a -> pEnumerationHandle = PR_enumIdToHandle(a -> pGroupHandle,
                               a -> iAttributeId);
            a -> iType = BIF_INT;
            a -> iMaxSize = sizeof(MIF_Int_t);

            return;
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_ILLEGAL_VALUE;
            goto attributeError;
        }
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_EQUALS;
        goto attributeError;
    }

attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
}

static void PR_pValueStmt(void)
{
    struct AttributeData *a;
    MIF_Date_t           *date;
    PR_ErrorNumber_t     errorCode;
    char                 *literal;
    PR_ParseStruct_t     *ps;
    size_t               size;
    PR_uString           (string, PR_LITERAL_MAX);
    unsigned long        stringSize;
    PR_Token_t           *t;

    for (a = AttributeData.next;
         a -> next != (struct AttributeData *) 0;
         a = a -> next)
        ;

/*
    Check Access
*/ 

    t = PR_tokenTableGetCurrent();
    if (a -> iAccess== BIF_UNKNOWN_ACCESS_TYPE)
        a -> iAccess = BIF_READ_ONLY;

/*
    Check Datatype
*/

    if (a -> iType == BIF_UNKNOWN_DATA_TYPE) {
        t = PR_tokenTableGetNext();
        errorCode = PR_MISSING_TYPE;
        goto attributeError;
    }

    if (PR_SUCCESSFUL(ps = PR_parseEquals())) {

/*
        Value = <literal>
*/

        if (PR_SUCCESSFUL(ps = PR_parseLiteral((char *) 0))) {
            if (a -> iAccess == BIF_WRITE_ONLY) {
                t = PR_tokenTableGetNext();
                errorCode = PR_WRITE_ONLY;
                goto attributeError;
            }
            t = &(ps -> token[0]);
            literal = PR_tokenLiteralGet(t);
            PR_stringInit((DMI_STRING *) string, literal);
            stringSize = PR_stringSize((DMI_STRING *) string);

/*
            Check for Date
*/

            if (a -> iType == BIF_DATE) {
                if (strlen(literal) == 0){
                    if ((PR_groupHandleToId(a -> pGroupHandle) ==
                        PR_COMPONENT_ID_GROUP) &&
                        (a -> iAttributeId ==
                        PR_INSTALLATION_ATTRIBUTE))
                        OS_getTime((DMI_TimeStamp_t _FAR *) literal);
                    else
                        strcpy(literal, "00000000000000.000000+000");
                }
                if (! PR_cDate(literal)) {
                    errorCode = PR_ILLEGAL_VALUE;
                    goto attributeError;
                }
                if ((a -> pValue = OS_Alloc(sizeof(MIF_Date_t))) ==
                    (void *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto attributeFatal;
                }
                date = (MIF_Date_t *) (a -> pValue);
                memcpy(date, literal, strlen(literal));
                memset(& date -> iPadding, 0, sizeof(date -> iPadding));
                return;
            }

/*
            Check for enumeration
*/

            if (a -> pEnumerationHandle != (void *) 0) {
                if (! PR_enumElementExists(a -> pEnumerationHandle,
                                         (DMI_STRING *) string)) {
                    errorCode = PR_ILLEGAL_VALUE;
                    goto attributeError;
                }
                if ((a -> pValue = OS_Alloc(sizeof(MIF_Int_t))) == (void *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto attributeFatal;
                }
                *((MIF_Int_t *) a -> pValue) =
                    PR_enumElementValue(a -> pEnumerationHandle,
                                        (DMI_STRING *) string);

                return;
            }

            if ((a -> iType != BIF_DISPLAYSTRING) &&
                (a -> iType != BIF_OCTETSTRING)) {
                errorCode = PR_ILLEGAL_VALUE;
                goto attributeError;
            }
            if (a -> iMaxSize == 0)
                a -> iMaxSize = stringSize;
            else if (a -> iMaxSize < sizeof(unsigned long) +
                                                  strlen(literal)) {
                errorCode = PR_ILLEGAL_SIZE;
                goto attributeError;
            }
            if ((a -> pValue = OS_Alloc((size_t) stringSize)) == (void *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto attributeFatal;
            }
            memcpy(a -> pValue, string, (size_t) stringSize);

            return;

/*
        Value = <number>
*/

        } else if (PR_SUCCESSFUL(
                        ps = PR_parseTokenSequence("cN", '-', (char *) 0))) {
            if (a -> iAccess == BIF_WRITE_ONLY) {
                t = PR_tokenTableGetNext();
                errorCode = PR_WRITE_ONLY;
                goto attributeError;
            }
            t = &(ps -> token[(ps -> tokenCount) - 1]);
            literal = PR_tokenLiteralGet(t);
            if ((a -> iType == BIF_COUNTER64) ||
                (a -> iType == BIF_INT64))
                size = sizeof(PR_Int64_t);
            else
                size = sizeof(MIF_Int_t);
            if ((a -> pValue = OS_Alloc(size)) == (void *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto attributeFatal;
            }
            if (ps -> tokenCount == 2) {
                if ((a -> iType != BIF_INT) &&
                    (a -> iType != BIF_INT64)) {
                    errorCode = PR_ILLEGAL_VALUE;
                    goto attributeError;
                }
                if ((a -> iType == BIF_COUNTER64) ||
                    (a -> iType == BIF_INT64))
                    memcpy(a -> pValue, PR_negation64(PR_asciiToInt64(literal)),
                           sizeof(PR_Int64_t));
                else
                    *((MIF_Int_t *) a -> pValue) = -1 * PR_atol(literal);
            } else
                if ((a -> iType == BIF_COUNTER64) ||
                    (a -> iType == BIF_INT64))
                    memcpy(a -> pValue, PR_asciiToInt64(literal),
                           sizeof(PR_Int64_t));
                else
                    *((MIF_Int_t *) a -> pValue) = PR_atol(literal);
             
            if ((a -> iType != BIF_COUNTER) &&
                (a -> iType != BIF_COUNTER64) &&
                (a -> iType != BIF_GAUGE) &&
                (a -> iType != BIF_INT) &&
                (a -> iType != BIF_INT64)) {
                t = &(ps -> token[0]);
                errorCode = PR_ILLEGAL_VALUE;
                goto attributeError;
            }

            return;

/*
        Value = * <instrumentation literal>
*/

        } else if (PR_SUCCESSFUL(
                        ps = PR_parseTokenSequence("CL", '*', (char *) 0))) {
            t = &(ps -> token[1]);

            if (a -> iMaxSize == 0) {
                if (a -> iAccess == BIF_READ_WRITE) {
                    errorCode = PR_MISSING_SIZE;
                    goto attributeError;
                }
                a -> iMaxSize = PR_LITERAL_MAX;
            }

            literal = PR_tokenLiteralGet(t);
            PR_stringInit((DMI_STRING *) string, literal);
            stringSize = PR_stringSize((DMI_STRING *) string);
            if (PR_pathExists((DMI_STRING *) string, (DMI_STRING *) 0)) {

/*
                Suppress missing instrumentation message.

                if (! PR_pathExists((DMI_STRING *) string, PR_pathTargetOs()))
                    PR_errorLogAdd(t -> line, t -> col, PR_ERROR +
                                   PR_PATH_ERROR + PR_MISSING_VALUE);
*/

                if ((a -> pValue = (DMI_STRING *)
                                   OS_Alloc((size_t) stringSize)) ==
                    (DMI_STRING *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto attributeFatal;
                }
                memcpy(a -> pValue, string, (size_t) stringSize);
                a -> iValueType = BIF_OVERLAY_TYPE;

                return;
            } else {
                errorCode = PR_EXPECTING_OVERLAY;
                goto attributeError;
            }
        } else if (PR_SUCCESSFUL(
                        ps = PR_parseTokenSequence("I", "Unsupported"))) {
            a -> iAccess = BIF_UNSUPPORTED;
            return;
        } else if (PR_SUCCESSFUL(
                        ps = PR_parseTokenSequence("I", "Unknown"))) {
/* wy10-1, keep old access
            a -> iAccess = BIF_UNKNOWN;
*/
            return;
        } else {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_VALUE;
            goto attributeError;
        }
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_EQUALS;
        goto attributeError;
    }

attributeFatal:

    if(t != (PR_Token_t *)NULL)
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_FATAL + PR_ATTRIBUTE_ERROR + PR_DATABASE_FAULT));
    else
        PR_errorLogAdd(0,0,(PR_ErrorNumber_t) (PR_FATAL + PR_ATTRIBUTE_ERROR + PR_DATABASE_FAULT));


attributeError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_ATTRIBUTE_ERROR + errorCode));
}

MIF_Bool_t PR_cDate(char *date)
{
    unsigned char fieldSizes[]  = {4, 2, 2, 2, 2, 2, 1, 6, 1, 3, 0};
    int           i;
    unsigned      j;
    MIF_Bool_t    omittedField;
    char          *p;
    char          *separators   = ".+-";

    if (strlen(date) != sizeof(MIF_Date_t) -
                        sizeof(((MIF_Date_t *) date) -> iPadding))
        return MIF_FALSE;

    p = date;
    for (i = 0; fieldSizes[i] != 0; ++i) {
        if (fieldSizes[i] == 1) {
            if ((*p == *separators) || ((*separators == '+') && (*p == '-')))
                ++separators;
            else
                return MIF_FALSE;
        } else {
            if (*p == '*')
                omittedField = MIF_TRUE;
            else
                omittedField = MIF_FALSE;
            for (j = 0; j < fieldSizes[i]; ++j)
                switch (omittedField) {
                    case MIF_TRUE:
                        if (p[j] != '*')
                            return MIF_FALSE;
                        break;
                    case MIF_FALSE:
                        if (! isdigit(p[j]))
                            return MIF_FALSE;
                        break;
                }
        }
        p += fieldSizes[i];
    }

    return MIF_TRUE;
}

BIF_AttributeAccess_t PR_attributeAccess(void *groupHandle,
                                         unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return (BIF_AttributeAccess_t) a -> iAccess;

    return BIF_UNKNOWN_ACCESS_TYPE;
}

unsigned long PR_attributeCount(void *groupHandle)
{
    struct AttributeData *a;
    unsigned long        attributeCount;

    attributeCount = 0;
    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if (a -> pGroupHandle == groupHandle)
            ++attributeCount;

    return attributeCount;
}

DMI_STRING *PR_attributeDescription(void *groupHandle,
                                    unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId)) {
            return PR_tokenTableStringGet(a -> descriptionPos);
        }

    return (DMI_STRING *) 0;
}

void *PR_attributeEnumHandle(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return (void *) (a -> pEnumerationHandle);

    return (void *) 0;
}

MIF_Bool_t PR_attributeExists(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return MIF_TRUE;

    return MIF_FALSE;
}

DMI_STRING *PR_attributeName(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return a -> pAttributeName;

    return (DMI_STRING *) 0;
}

unsigned long PR_attributeNext(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;
    unsigned long        currentId;
    unsigned long        foundId;

    foundId = 0;
    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if (a -> pGroupHandle == groupHandle) {
            currentId = a -> iAttributeId;
            if (((foundId == 0) && (currentId > attributeId)) ||
                ((currentId < foundId) && (currentId > attributeId)))
                foundId = currentId;
        }

    return foundId;
}

unsigned long PR_attributeMaxSize(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return a -> iMaxSize;

    return 0;
}

BIF_AttributeStorage_t PR_attributeStorage(void *groupHandle,
                                           unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
                return (BIF_AttributeStorage_t) a -> iStorage;

    return BIF_UNKNOWN_STORAGE_TYPE;
}

BIF_AttributeDataType_t PR_attributeType(void *groupHandle,
                                         unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return (BIF_AttributeDataType_t) (a -> iType);

    return BIF_UNKNOWN_DATA_TYPE;
}

void *PR_attributeValue(void *groupHandle, unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return a -> pValue;

    return (void *) 0;
}

BIF_AttributeValueType_t PR_attributeValueType(void *groupHandle,
                                               unsigned long attributeId)
{
    struct AttributeData *a;

    for (a = AttributeData.next; a != (struct AttributeData *) 0; a = a -> next)
        if ((a -> pGroupHandle == groupHandle) &&
            (a -> iAttributeId == attributeId))
            return (BIF_AttributeValueType_t) a -> iValueType;

    return BIF_VALUE_TYPE;
}
