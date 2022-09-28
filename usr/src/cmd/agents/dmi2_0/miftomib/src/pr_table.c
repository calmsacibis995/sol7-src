/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_table.c	1.4 96/10/02 Sun Microsystems"


/**********************************************************************
    Filename: pr_table.c

    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Parser Static Table routines

    Author(s): Alvin I. PIvowar
               Paul A. Ruocchio

    RCS Revision: $Header: j:/mif/parser/rcs/pr_table.c 1.26 1994/10/28 15:17:40 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        6/22/93  aip    Added call to DB_groupCopy for template support.
        7/23/93  aip    Use new identification parsing.
                        Added octal and hexadecimal number support.
        7/28/93  aip    Changed masks to MIF_...
        7/30/93  aip    Fixed optional id test with identification parsing.
        11/2/93  aip    Use new MIF_dataValueGet()
        11/8/93  aip    Add table elements for skipped columns, if those
                        attributes need "placeholders" in the writable database.
        12/3/93  aip    Illegal to associate a table with a non-keyed group.
        12/22/93 aip    Enumerations are literals.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit strings.
        4/21/94  aip    Added 64-bit types.
        6/1/94   aip    Suppressed missing base value error with unsupported
                        attributes.
        6/10/94  aip    Fixed enumeration problem.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        7/22/94  aip    Suppress missing instrumentation error.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/2/94   aip    Use enumeration handles.
        8/4/94   aip    Enforce that a table row has values for every column.
        8/29/94  aip    ANSI-compliant.
        9/2/94   aip    Added Date types to tables.
        10/28/94 aip    Added component ID group checking.
                        Fixed date discriminate checking.
        11/14/94 aip    Fixed defaulting of unsupported table values.
        11/17/94 par    Fixed support for 64 bit types

************************* INCLUDES ***********************************/

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
#include "pr_plib.h"
#include "pr_table.h"
#include "os_svc.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static void PR_cTable(void);
static void PR_dTable(void);
static void PR_eTable(void);
static void PR_pTableValues(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

struct TableElement {
    struct TableElement *next;
    unsigned long       iAttributeId;
    unsigned long       iValueType;
    unsigned long       iMaxSize;
    void                *pValue;
};

static struct TableData {
    struct TableData    *next;
    unsigned long       iTableId;
    struct TableElement elementList;
} TableData;

/*********************************************************************/

void PR_tableInit(void)
{
    struct TableData    *ta;
    struct TableElement *te;

    while (TableData.next) {
        ta = TableData.next;
        while (ta -> elementList.next != (struct TableElement *) 0) {
            te = ta -> elementList.next;
            if (te -> pValue != (void *) 0)
                OS_Free(te -> pValue);
            ta -> elementList.next = te -> next;
            OS_Free(te);
        }
        TableData.next = ta -> next;
        OS_Free(ta);
    }
    memset(&TableData,0,sizeof(struct TableData));   /* clear out the table itself */
}

void PR_pTable(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;
    PR_Token_t       *idToken;
    char             *literal;
    PR_uString       (tableName, PR_LITERAL_MAX);
    unsigned long    tableId;
    PR_uString       (class, PR_LITERAL_MAX);
    void             *groupHandle;
    void             *tableHandle;
    struct TableData *tbl;
    int              i;

    t = (PR_Token_t *) 0;
    idToken = (PR_Token_t *) 0;
    tableId = 0;
    groupHandle = (void *) 0;

    Scope = PR_TABLE_SCOPE;
    tbl = (struct TableData *) 0;

/*
    Name = <literal>
    ID = <number>
    Class = <literal>
*/

    if (PR_UNSUCCESSFUL(
        ps = PR_pIdentification("LIL", Ks.class, Ks.id, Ks.name))) {
        PR_eTable();
        return;
    }

    for (i = 0; i < (int) strlen("LiL"); ++i) {
        t = &(ps -> token[i]);
        literal = PR_tokenLiteralGet(t);
        switch (t -> type) {
            case PR_TOK_LITERAL:
                if (ps -> tag[i] == 0) {
                    PR_stringInit((DMI_STRING *) class, literal);
                    groupHandle = PR_groupClassToHandle((DMI_STRING *) class);
                    if (groupHandle == (void *) 0) {
                        errorCode = PR_EXPECTING_TEMPLATE_CLASS;
                        goto tableError;
                    }
                } else
                    PR_stringInit((DMI_STRING *) tableName, literal);
                break;
            case PR_TOK_NUMBER:
                idToken = t;
                tableId = PR_atol(literal);
                break;
        }
    }
    t = idToken;
    if (PR_groupExists(tableId)) {
            errorCode = PR_CONFLICTING;
            goto tableError;
    }
    if ((tableId == PR_COMPONENT_ID_GROUP) &&
        (strncmp(((DMI_STRING *) class) -> body,
                 PR_COMPONENT_ID_GROUP_CLASS_STRING,
                 strlen(PR_COMPONENT_ID_GROUP_CLASS_STRING) != 0)))
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_WARN + PR_TABLE_ERROR + PR_CID_ERROR));

    if (PR_groupId(groupHandle) == 0) {
        tableHandle = groupHandle;
        PR_groupIdStore(tableHandle, tableId);
    } else
        tableHandle = PR_groupCopy(tableId, groupHandle);

    PR_groupNameStore(tableHandle, (DMI_STRING *) tableName);
    PR_groupTemplateHandleStore(tableHandle, groupHandle);

/*
    Create a new table in the list
*/

    for (tbl = &TableData;
         tbl -> next != (struct TableData *) 0;
         tbl = tbl -> next)
        ;
    if ((tbl -> next =
         (struct TableData *) OS_Alloc(sizeof(struct TableData))) ==
         (struct TableData *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto tableFatal;
    }
    tbl = tbl -> next;
    tbl -> next = (struct TableData *) 0;
    tbl -> iTableId = tableId;
    tbl -> elementList.next = (struct TableElement *) 0;

/*
        <table line>
*/

    if (PR_SUCCESSFUL(ps = PR_parseLeftBrace())) {
        PR_pTableValues();
        PR_cTable();
    } else
        PR_eTable();

    return;

tableFatal:

    if(t != (PR_Token_t *)NULL)
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_FATAL + PR_TABLE_ERROR + PR_DATABASE_FAULT));
    else
        PR_errorLogAdd(0, 0,(PR_ErrorNumber_t) (PR_FATAL + PR_TABLE_ERROR + PR_DATABASE_FAULT));

tableError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_TABLE_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.table);
    if (tbl != (struct TableData *) 0)
        PR_dTable();
}

static void PR_cTable()
{
    PR_Token_t       *t;
    struct TableData *tbl;

    for (tbl = TableData.next;
         tbl -> next != (struct TableData *) 0;
         tbl = tbl -> next)
        ;
    if (tbl == (struct TableData *) 0)
        return;

    t = PR_tokenTableGetCurrent();
/* wy10-1
    if (tbl -> elementList.next == (struct TableElement *) 0) {
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_ERROR + PR_TABLE_ERROR + PR_MISSING_BODY));
        PR_dTable();
    }
*/
}

static void PR_dTable(void)
{
    struct TableData    *tbl;
    struct TableElement *el;

    if (TableData.next == (struct TableData *) 0)
        return;
    for (tbl = &TableData;
         tbl -> next -> next != (struct TableData *) 0;
         tbl = tbl -> next)
        ;

    while (tbl -> next -> elementList.next != (struct TableElement *) 0) {
        for (el = &tbl -> next -> elementList;
             el -> next -> next != (struct TableElement *) 0;
             el = el -> next)
            ;
        if (el -> next -> pValue != (void *) 0)
            OS_Free(el -> next -> pValue);
        OS_Free(el -> next);
        el -> next = (struct TableElement *) 0;
    }
    OS_Free(tbl -> next);
    tbl -> next = (struct TableData *) 0;
}

static void PR_eTable(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.name))) {
        t = &(ps -> token[0]);
        if (PR_SUCCESSFUL(ps = PR_parseEquals()))
            errorCode = PR_EXPECTING_LITERAL;
        else
            errorCode = PR_EXPECTING_EQUALS;
    } else if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.id))) {
        t = &(ps -> token[0]);
        if (PR_SUCCESSFUL(ps = PR_parseEquals()))
            errorCode = PR_EXPECTING_INTEGER;
        else
            errorCode = PR_EXPECTING_EQUALS;
    } else if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.class))) {
        t = &(ps -> token[0]);
        if (PR_SUCCESSFUL(ps = PR_parseEquals()))
            errorCode = PR_EXPECTING_LITERAL;
        else
            errorCode = PR_EXPECTING_EQUALS;
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_STATEMENT;
    }

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_TABLE_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.table);
    PR_dTable();
    return;
}

static void PR_pTableValues(void)
{
    typedef enum {TB_BRACE, TB_COMMA, TB_VALUE} TB_TokenType_t;

    PR_ErrorNumber_t         errorCode;
    TB_TokenType_t           lastToken;
    PR_ParseStruct_t         *ps;
    PR_Token_t               *t;
    struct TableData         *tbl;
    struct TableElement      *el;
    struct TableElement      *tableRow;
    MIF_Bool_t               keyMatch;
    unsigned long            matchCount;
    unsigned long            groupId;
    void                     *groupHandle;
    void                     *templateHandle;
    void                     *enumHandle;
    unsigned long            attributeCount;
    unsigned long            attributeId;
    int			     maxSize;
    BIF_AttributeDataType_t  dataType;
    unsigned long            columnCount;
    unsigned long            columnNumber;
    unsigned long            rowNumber;
    char                     *literal;
    unsigned long            iValue;
    void                     *value;
    unsigned long            valueSize = 0;
    BIF_AttributeValueType_t valueType;
    PR_uString               (string, PR_LITERAL_MAX);
    MIF_Date_t               *date;
    unsigned long            i;
    unsigned long           iAccessType;

    t = (PR_Token_t *) 0;
    tableRow = (struct TableElement *) 0;

    for (tbl = TableData.next;
         tbl -> next != (struct TableData *) 0;
         tbl = tbl -> next)
        ;
    groupId = tbl -> iTableId;
    groupHandle = PR_groupIdToHandle(groupId);
    templateHandle = PR_groupTemplateHandle(groupHandle);   /* grab the template handle here */

    attributeCount = PR_attributeCount(templateHandle);

/*
    Build a table row for checking uniqueness
*/

    if ((tableRow = (struct TableElement *) OS_Alloc((size_t) (attributeCount *
                sizeof(struct TableElement)))) == (struct TableElement *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto tableFatal;
    }
    attributeId = 0;
    i = 0;
    while ((attributeId = PR_attributeNext(templateHandle, attributeId)) != 0) {
        tableRow[i].iAttributeId = attributeId;
        maxSize = PR_attributeMaxSize(templateHandle, attributeId);
        tableRow[i].iMaxSize = (unsigned long)maxSize;
        if (maxSize > 0)
            if ((tableRow[i].pValue = OS_Alloc((size_t) maxSize)) ==
                (void *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto tableFatal;
            }
        ++i;
    }

    lastToken = TB_BRACE;
    t = PR_tokenTableGetCurrent();
    rowNumber = 1;
    columnCount = 0;
    columnNumber = PR_attributeNext(templateHandle, 0);

    for (;;) {

/*
        "}"     (Right Brace)
*/

        if (PR_SUCCESSFUL(ps = PR_parseRightBrace())) {
            t = &(ps -> token[0]);
            if (lastToken == TB_BRACE) {
/* wy10-1
                errorCode = PR_EXPECTING_VALUE;
                goto tableError;
*/
            	if (PR_SUCCESSFUL(PR_parseTokenSequence("KK", Ks.end, Ks.table))) {
                	OS_Free(tableRow);
                	return;
            	}
            }

/*
            Was the last term defaulted?
*/

            if (lastToken != TB_VALUE) {
                value = PR_attributeValue(templateHandle, columnNumber);
                valueType = PR_attributeValueType(templateHandle, columnNumber);
                for (i = 0; tableRow[i].iAttributeId != columnNumber; ++i)
                    if (tableRow[i].iValueType != (unsigned long) valueType) {
                        errorCode = PR_MIXED_ROW;
                        goto tableError;
                    }
                tableRow[i].iValueType = valueType;
                if (value == (void *) 0) {
                    iAccessType = PR_attributeAccess(templateHandle, columnNumber);
                    if ((iAccessType != BIF_UNSUPPORTED)  && (iAccessType != BIF_UNKNOWN) ){
                        errorCode = PR_NO_DEFAULT_VALUE;
                        goto tableError;
                    }
                } else {
                    dataType = PR_attributeType(templateHandle, columnNumber);
                    switch(dataType){
                        case BIF_DISPLAYSTRING:
                        case BIF_OCTETSTRING:
                            valueSize = PR_stringSize((DMI_STRING *) value);
                            break;
                        case BIF_DATE:
                            valueSize = sizeof(MIF_Date_t);
                            break;
                        case BIF_COUNTER:
                        case BIF_GAUGE:
                        case BIF_INT:
                            valueSize = 4;
                            break;
                        case BIF_COUNTER64:
                        case BIF_INT64:
                            valueSize = 8;
                            break;
                    }
                    memcpy(tableRow[i].pValue, value, (size_t) valueSize);

                    for (el = &(tbl -> elementList);
                         el -> next != (struct TableElement *) 0;
                         el = el -> next)
                        ;
                    if ((el -> next = (struct TableElement *)
                        OS_Alloc(sizeof(struct TableElement))) ==
                        (struct TableElement *) 0) {
                        errorCode = PR_OUT_OF_MEMORY;
                        goto tableFatal;
                    }
                    el = el -> next;
                    el -> next = (struct TableElement *) 0;
                    el -> iAttributeId = columnNumber;
                    el -> iValueType = valueType;
                    if ((el -> pValue = OS_Alloc((size_t) valueSize)) ==
                        (void *) 0) {
                        errorCode = PR_OUT_OF_MEMORY;
                        goto tableFatal;
                    }
                    memcpy(el -> pValue, value, (size_t) valueSize);
                }
                ++columnCount;
            }

/*
            Were all the columns accounted for?
*/

            if (columnCount != attributeCount) {
                errorCode = PR_MISSING_VALUE;
                goto tableError;
            }

/*
            Check key for uniqueness
*/

            matchCount = 0;
            el = tbl -> elementList.next;
            while (el != (struct TableElement *) 0) {
                keyMatch = MIF_TRUE;
                for (i = 0; i < PR_attributeCount(templateHandle); ++i) {
                    attributeId = tableRow[i].iAttributeId;
                    if (PR_groupKeyExists(templateHandle, attributeId)) {
                        while ((el != (struct TableElement *) 0) &&
                               (el -> iAttributeId != attributeId))
                            el = el -> next;
                        if (el == (struct TableElement *) 0)
                            break;
                        if (el -> iValueType == tableRow[i].iValueType) {
                            if (tableRow[i].iValueType == BIF_VALUE_TYPE) {
                                dataType = PR_attributeType(templateHandle,attributeId);
                                switch(dataType){
                                    case BIF_DISPLAYSTRING:
                                    case BIF_OCTETSTRING:
                                        if (PR_stringCmp(el -> pValue,
                                                  tableRow[i].pValue) != 0)
                                            keyMatch = MIF_FALSE;
                                        break;
                                    case BIF_DATE:
                                        if(memcmp(el->pValue,tableRow[i].pValue,sizeof(MIF_Date_t)) != 0)
                                            keyMatch = MIF_FALSE;
                                        break;
                                    case BIF_COUNTER64:
                                    case BIF_INT64:
                                        if(memcmp(el->pValue,tableRow[i].pValue,8) != 0)
                                            keyMatch = MIF_FALSE;
                                        break;
                                    default:
                                        if (*((MIF_Int_t *) el -> pValue) !=
                                            *((MIF_Int_t *) tableRow[i].pValue))
                                            keyMatch = MIF_FALSE;
                                        break;
                                }
                            }
                        } else
                            keyMatch = MIF_FALSE;
                    }
                }
                if (el != (struct TableElement *) 0) {
                    if (keyMatch)
                        ++matchCount;
                    el = el -> next;
                }
            }
            if (matchCount > 1) {
                errorCode = PR_ROW_NOT_UNIQUE;
                goto tableError;
            }

/*
            End Table
*/

            if (PR_SUCCESSFUL(PR_parseTokenSequence("KK", Ks.end, Ks.table))) {
                OS_Free(tableRow);
                return;
            }

/*
            "{"     (Left Brace)
*/

            if (PR_SUCCESSFUL(ps = PR_parseLeftBrace())) {
                ++rowNumber;
                columnCount = 0;
                columnNumber = PR_attributeNext(templateHandle, 0);
            } else {
                t = &(ps -> token[0]);
                errorCode = PR_EXPECTING_STATEMENT;
                goto tableError;
            }
            lastToken = TB_BRACE;

/*
        ","     (Comma)
*/

        } else if (PR_SUCCESSFUL(ps = PR_parseComma())) {
            t = &(ps -> token[0]);

/*
            Was the last term defaulted?
*/

            if (lastToken != TB_VALUE) {
                value = PR_attributeValue(templateHandle, columnNumber);
                valueType = PR_attributeValueType(templateHandle, columnNumber);
                for (i = 0; tableRow[i].iAttributeId != columnNumber; ++i)
                    if (tableRow[i].iValueType != (unsigned long) valueType) {
                        errorCode = PR_MIXED_ROW;
                        goto tableError;
                    }
                tableRow[i].iValueType = valueType;
                if (value == (void *) 0) {
                    iAccessType = PR_attributeAccess(templateHandle, columnNumber);
                    if ((iAccessType != BIF_UNSUPPORTED)  && (iAccessType != BIF_UNKNOWN) ){
                        errorCode = PR_NO_DEFAULT_VALUE;
                        goto tableError;
                    }
                } else {
                    dataType = PR_attributeType(templateHandle, columnNumber);
                    switch(dataType){
                        case BIF_DISPLAYSTRING:
                        case BIF_OCTETSTRING:
                            valueSize = PR_stringSize((DMI_STRING *) value);
                            break;
                        case BIF_DATE:
                            valueSize = sizeof(MIF_Date_t);
                            break;
                        case BIF_COUNTER64:
                        case BIF_INT64:
                            valueSize = 8;
                            break;
                        default:
                            valueSize = 4;
                            break;
                    }
                    memcpy(tableRow[i].pValue, value, (size_t) valueSize);

                    for (el = &(tbl -> elementList);
                         el -> next != (struct TableElement *) 0;
                         el = el -> next)
                        ;
                    if ((el -> next = (struct TableElement *)
                        OS_Alloc(sizeof(struct TableElement))) ==
                        (struct TableElement *) 0) {
                        errorCode = PR_OUT_OF_MEMORY;
                        goto tableFatal;
                    }
                    el = el -> next;
                    el -> next = (struct TableElement *) 0;
                    el -> iAttributeId = columnNumber;
                    el -> iValueType = valueType;
                    if ((el -> pValue = OS_Alloc((size_t) valueSize)) ==
                        (void *) 0) {
                        errorCode = PR_OUT_OF_MEMORY;
                        goto tableFatal;
                    }
                    memcpy(el -> pValue, value, (size_t) valueSize);
                }
                ++columnCount;
            }

            columnNumber = PR_attributeNext(templateHandle, columnNumber);
            if (columnNumber == 0) {
                errorCode = PR_TOO_MANY_VALUES;
                goto tableError;
            }
            lastToken = TB_COMMA;

/*
        <value>
*/

        } else {
            t = &(ps -> token[0]);
            if (lastToken == TB_VALUE) {
                errorCode = PR_EXPECTING_SEPARATOR;
                goto tableError;
            }
            valueType = BIF_VALUE_TYPE;

/*
            <literal>
*/

            if (PR_SUCCESSFUL(ps = PR_parseLiteral((char *) 0))) {
                t = &(ps -> token[0]);
                literal = PR_tokenLiteralGet(t);
                PR_stringInit((DMI_STRING *) string, literal);

/*
                Is a value legal?
*/

                for (i = 0; tableRow[i].iAttributeId != columnNumber; ++i)
                    if (tableRow[i].iValueType != (unsigned long) valueType) {
                        errorCode = PR_MIXED_ROW;
                        goto tableError;
                    }

/*
                Check for enumeration
*/

                enumHandle = PR_attributeEnumHandle(templateHandle,
                                                    columnNumber);
                if (enumHandle != (void *) 0) {
                    if (PR_enumElementExists(enumHandle,
                                             (DMI_STRING *) string)) {
                        iValue = PR_enumElementValue(enumHandle,
                                                     (DMI_STRING *) string);
                        value = &iValue;
                        valueSize = sizeof(MIF_Int_t);
                    } else {
                        errorCode = PR_ILLEGAL_VALUE;
                        goto tableError;
                    }
                } else {
                    dataType = PR_attributeType(templateHandle, columnNumber);
                    switch(dataType){
                        case BIF_DISPLAYSTRING:
                        case BIF_OCTETSTRING:
                            value = &string;
                            valueSize = PR_stringSize((DMI_STRING *) string);
                            break;
                        case BIF_DATE:
                            if (strlen(literal) == 0)
                                strcpy(literal, "00000000000000.000000+000");
                            if (! PR_cDate(literal)) {
                                errorCode = PR_ILLEGAL_VALUE;
                                goto tableError;
                            }
                            date = (MIF_Date_t *) literal;
                            memset(& date -> iPadding, 0, sizeof(date -> iPadding));
                            value = literal;
                            valueSize = sizeof(MIF_Date_t);
                            break;
                         default: 
                            errorCode = PR_ILLEGAL_VALUE;
                            goto tableError;
                    }
                    if (valueSize > PR_attributeMaxSize(templateHandle,
                                                        columnNumber)) {
                        errorCode = PR_ILLEGAL_SIZE;
                        goto tableError;
                    }
                }

/*
            <number>
*/

            } else if (PR_SUCCESSFUL(
                          ps = PR_parseTokenSequence("cN", '-', (char *) 0))) {
                t = &(ps -> token[0]);
                dataType = PR_attributeType(templateHandle, columnNumber);
                if ((dataType != BIF_COUNTER) &&
                    (dataType != BIF_COUNTER64) &&
                    (dataType != BIF_GAUGE) &&
                    (dataType != BIF_INT) &&
                    (dataType != BIF_INT64)) {
                    errorCode = PR_ILLEGAL_VALUE;
                    goto tableError;
                }
                if (ps -> tokenCount == 2) {
                    if ((dataType != BIF_INT) && (dataType != BIF_INT64)) {
                        errorCode = PR_ILLEGAL_VALUE;
                        goto tableError;
                    }
                    t = &(ps -> token[1]);
                    literal = PR_tokenLiteralGet(t);
                    if ((dataType == BIF_COUNTER64) ||
                        (dataType == BIF_INT64)) {
                        value = PR_negation64(PR_asciiToInt64(literal));
                        valueSize = sizeof(PR_Int64_t);
                    } else {
                        iValue = -1 * PR_atol(literal);
                        value = &iValue;
                        valueSize = sizeof(MIF_Int_t);
                    }
                } else {
                    literal = PR_tokenLiteralGet(t);
                    if ((dataType == BIF_COUNTER64) ||
                        (dataType == BIF_INT64)) {
                        value = PR_asciiToInt64(literal);
                        valueSize = sizeof(PR_Int64_t);
                    } else {
                        iValue = PR_atol(literal);
                        value = &iValue;
                        valueSize = sizeof(MIF_Int_t);
                    }
                }

/*
                Is a value legal?
*/

                for (i = 0; tableRow[i].iAttributeId != columnNumber; ++i)
                    if (tableRow[i].iValueType != (unsigned long) valueType) {
                        errorCode = PR_MIXED_ROW;
                        goto tableError;
                    }

/*
            * <overlay name>
*/

            } else if (PR_SUCCESSFUL(
                          ps = PR_parseTokenSequence("CL", '*', (char *) 0))) {
                valueType = BIF_OVERLAY_TYPE;

/*
                Is an overlay name legal?
*/

                for (i = 0; tableRow[i].iAttributeId != columnNumber; ++i)
                    if (tableRow[i].iValueType != (unsigned long) valueType) {
                        errorCode = PR_MIXED_ROW;
                        goto tableError;
                    }

                t = &(ps -> token[1]);
                literal = PR_tokenLiteralGet(t);
                PR_stringInit((DMI_STRING *) string, literal);
                if (! PR_pathExists((DMI_STRING *) string,
                                    (DMI_STRING *) 0)) {
                    errorCode = PR_EXPECTING_OVERLAY;
                    goto tableError;
                }

/*
                Suppress missing instrumentation error.

                if (! PR_pathExists((DMI_STRING *) string, PR_pathTargetOs()))
                    PR_errorLogAdd(t -> line, t -> col, (PR_ErrorNumber_t)
                        (PR_ERROR + PR_PATH_ERROR + PR_MISSING_VALUE));
*/

                value = &string;
                valueSize = PR_stringSize((DMI_STRING *) string);
            } else {
                t = &(ps -> token[0]);
                errorCode = PR_EXPECTING_VALUE;
                goto tableError;
            }

            for (el = &(tbl -> elementList);
                 el -> next != (struct TableElement *) 0;
                 el = el -> next)
                ;
            if ((el -> next = (struct TableElement *)
                              OS_Alloc(sizeof(struct TableElement))) ==
                              (struct TableElement *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto tableFatal;
            }
            el = el -> next;
            el -> next = (struct TableElement *) 0;
            el -> iAttributeId = columnNumber;
            tableRow[i].iValueType = valueType;
            el -> iValueType = valueType;
            if ((el -> pValue = OS_Alloc((size_t) valueSize)) == (void *) 0) {
                errorCode = PR_OUT_OF_MEMORY;
                goto tableFatal;
            }
            if((size_t)valueSize > tableRow[i].iMaxSize){  /* we need to reallocate the value space */
                OS_Free(tableRow[i].pValue);
                if ((tableRow[i].pValue = OS_Alloc((size_t) valueSize)) == (void *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto tableFatal;
                }
            }
            memcpy(tableRow[i].pValue, value, (size_t) valueSize);
            memcpy(el -> pValue, value, (size_t) valueSize);
            lastToken = TB_VALUE;
            ++columnCount;
        }
    }

tableFatal:

    if(t != (PR_Token_t *)NULL)
        PR_errorLogAdd(t -> line, t -> col,
            (PR_ErrorNumber_t) (PR_FATAL + PR_TABLE_ERROR + PR_DATABASE_FAULT));
    else
        PR_errorLogAdd(0, 0,(PR_ErrorNumber_t) (PR_FATAL + PR_TABLE_ERROR + PR_DATABASE_FAULT));


tableError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_TABLE_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.table);
    OS_Free(tableRow);
}

unsigned long PR_tableElementCount(unsigned long tableId)
{
    struct TableData    *t;
    struct TableElement *e;
    unsigned long elementCount;

    for (t = TableData.next; t != (struct TableData *) 0; t = t -> next)
        if (t -> iTableId == tableId) {
            elementCount = 0;
            for (e = t -> elementList.next;
                 e != (struct TableElement *) 0;
                 e = e -> next)
                ++elementCount;

            return elementCount;
        }

    return 0;
}

unsigned long PR_tableElementId(unsigned long tableId,
                                unsigned long elementIndex)
{
    struct TableData    *t;
    struct TableElement *e;

    for (t = TableData.next; t != (struct TableData *) 0; t = t -> next)
        if (t -> iTableId == tableId) {
            for (e = t -> elementList.next;
                 e != (struct TableElement *) 0;
                 e = e -> next)
                if (--elementIndex == 0)
                    return e -> iAttributeId;

            return 0;
        }

    return 0;
}

void *PR_tableElementValue(unsigned long tableId, unsigned long elementIndex)
{
    struct TableData    *t;
    struct TableElement *e;

    for (t = TableData.next; t != (struct TableData *) 0; t = t -> next)
        if (t -> iTableId == tableId) {
            for (e = t -> elementList.next;
                 e != (struct TableElement *) 0;
                 e = e -> next)
                if (--elementIndex == 0)
                    return e -> pValue;

            return (void *) 0;
        }

    return (void *) 0;
}

BIF_AttributeValueType_t PR_tableElementValueType(unsigned long tableId,
                                                  unsigned long elementIndex)
{
    struct TableData    *t;
    struct TableElement *e;

    for (t = TableData.next; t != (struct TableData *) 0; t = t -> next)
        if (t -> iTableId == tableId) {
            for (e = t -> elementList.next;
                 e != (struct TableElement *) 0;
                 e = e -> next)
                if (--elementIndex == 0)
                    return (BIF_AttributeValueType_t) e -> iValueType;

            return BIF_VALUE_TYPE;
        }

    return BIF_VALUE_TYPE;
}

