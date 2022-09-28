/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_group.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_group.c
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF parser group routines

    Author(s): Alvin I. PIvowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_group.c 1.16 1994/08/29 15:51:44 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Use new identification parsing.
                        Added octal and hexadecimal support.
        12/3/93  aip    Added class statement checking.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    32-bit strings.
        5/24/94  aip    Fixed PR_cClass()
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/4/94   aip    Keep descriptions in token table.
        8/29/94  aip    ANSI-compliant.
        10/28/94 aip    Fixed class string checking.
                        Use group handles.
        10/10/95 par    Modified to remove dead code

************************* INCLUDES ***********************************/

#include <stdlib.h>
#include <string.h>
#include "db_api.h"
#include "db_int.h"
#include "mif_db.h"
#include "pr_attr.h"
#include "pr_group.h"
#include "pr_key.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_plib.h"
#include "os_svc.h"
#include "pr_todmi.h"

/*********************************************************************/

/************************ PRIVATE ************************************/

static void PR_cClass(PR_Token_t *t, DMI_STRING  *class,
                      unsigned long groupId);
static void PR_cGroup(PR_Token_t *t);
static void PR_dGroup(void);
static void PR_eGroup(void);
static void PR_pKeyStmt(void);
static void PR_cKey(PR_Token_t *t);
static PR_ParseStruct_t *PR_pGroupPragma(MIF_Pos_t *literalPos);
/*********************************************************************/

/************************ GLOBALS ************************************/

extern PR_KeywordStruct_t Ks;
extern PR_Scope_t         Scope;

static struct GroupData {
    struct GroupData *next;
    DMI_STRING       *pGroupName;
    DMI_STRING       *pClass;
    MIF_Pos_t        descriptionPos;
    MIF_Pos_t        pragmaPos;
    unsigned long    *iKeyList;
    unsigned long    iGroupId;
    unsigned long    osGroupName;
    unsigned long    osClass;
    unsigned long    osDescription;
    unsigned long    iKeyCount;
    unsigned long    osKeyList;
    unsigned long    iAttributeCount;
    unsigned long    osAttributeList;
    unsigned long    iDataCount;
    unsigned long    osDataList;
    void             *pTemplateHandle;
} GroupData;

/*********************************************************************/

void PR_groupInit(void)
{
    struct GroupData *g;

    while (GroupData.next != 0) {
        g = GroupData.next;
        if (g -> pGroupName != (DMI_STRING *) 0)
            OS_Free(g -> pGroupName);
        if (g -> pClass != (DMI_STRING *) 0)
            OS_Free(g -> pClass);
        if (g -> iKeyList != (unsigned long *) 0)
            OS_Free(g -> iKeyList);
        GroupData.next = g -> next;
        OS_Free(g);
    }
    memset(&GroupData,0,sizeof(struct GroupData));
}

void PR_pGroup(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;
    PR_Token_t       *classToken;
    PR_Token_t       *idToken;
    unsigned long    groupId;
    struct GroupData *g;
    char             *literal;
    unsigned long    stringSize;
    PR_uString       (class, PR_LITERAL_MAX);
    PR_uString       (groupName, PR_LITERAL_MAX);
    int              i;

    g = (struct GroupData *) 0;
    t = (PR_Token_t *) 0;
    idToken = (PR_Token_t *) 0;
    classToken = (PR_Token_t *) 0;

    Scope = PR_GROUP_SCOPE;

/*
    name = <literal>
    id = <integer>
*/

    if (PR_UNSUCCESSFUL(
            ps = PR_pIdentification("LiL", Ks.class, Ks.id, Ks.name)))
        return;
    groupId = 0;
    for (i = 0; i < (int) ps -> tokenCount; ++i) {
        t = &(ps -> token[i]);
        literal = PR_tokenLiteralGet(t);
        switch (t -> type) {
            case PR_TOK_NUMBER:
                idToken = t;
                groupId = PR_atol(literal);
                break;
            case PR_TOK_LITERAL:
                if (ps -> tag[i] == 0) {
                    classToken = t;
                    PR_stringInit((DMI_STRING *) class, literal);
                } else
                    PR_stringInit((DMI_STRING *) groupName, literal);
                break;
        }
    }

/*
    Check for a duplicate ID for non templates
*/

    if (groupId != 0)
        for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
            if (g -> iGroupId == groupId) {
                t = idToken;
                errorCode = PR_CONFLICTING;
                goto groupError;
            }

/*
    Check the class string
*/

    PR_cClass(classToken, (DMI_STRING *) class, groupId);

/*
    Add a group to the list
*/

    for (g = &GroupData; g -> next != (struct GroupData *) 0; g = g -> next)
        ;
    if ((g -> next = (struct GroupData *) OS_Alloc(sizeof(struct GroupData))) ==
        (struct GroupData *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto groupFatal;
    }
    g = g -> next;
    g -> next = (struct GroupData *) 0;
    g -> pClass = (DMI_STRING *) 0;
    g -> iGroupId = groupId;
    g -> iKeyCount = 0;
    stringSize = PR_stringSize((DMI_STRING *) groupName);
    if ((g -> pGroupName = (DMI_STRING *) OS_Alloc((size_t) stringSize)) ==
        (DMI_STRING *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto groupFatal;
    }
    memcpy(g -> pGroupName, groupName, (size_t) stringSize);
    stringSize = PR_stringSize((DMI_STRING *) class);
    if ((g -> pClass = (DMI_STRING *) OS_Alloc((size_t) stringSize)) ==
        (DMI_STRING *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto groupFatal;
    }
    memcpy(g -> pClass, class, (size_t) stringSize);
    g -> descriptionPos.page = 0;
    g -> descriptionPos.offset = 0;
    g -> pragmaPos.page = 0;
    g -> pragmaPos.offset = 0;
    g -> iKeyList = (unsigned long *) 0;
    g -> pTemplateHandle = (void *) 0;

    for (;;) {
        Scope = PR_GROUP_SCOPE;

/*
        Start Attribute
*/

        if (PR_SUCCESSFUL(
                ps = PR_parseTokenSequence("KK", Ks.start, Ks.attribute)))
            PR_pAttribute(g);

/*
        Key = <int> [{, <int>}*]
*/

        else if (PR_SUCCESSFUL(PR_parseKeyword(Ks.key)))
            PR_pKeyStmt();


/*
        PRAGMA  <literal>
*/

        else if (PR_SUCCESSFUL(PR_pGroupPragma(&g -> pragmaPos)))
            ;



/*
        Description <literal>
*/

        else if (PR_SUCCESSFUL(PR_pDescription(&g -> descriptionPos)))
            ;

/*
        End Group
*/

        else if (PR_SUCCESSFUL(
                    ps = PR_parseTokenSequence("KK", Ks.end, Ks.group))) {
            t = &(ps -> token[1]);
            PR_cGroup(t);
            return;
        } else {
            PR_eGroup();
            return;
        }
    }

groupFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_GROUP_ERROR + errorCode));

groupError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_GROUP_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.group);
    if (g != (struct GroupData *) 0)
        PR_dGroup();
}

static PR_ParseStruct_t *PR_pGroupPragma(MIF_Pos_t *literalPos)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.pragma))) {
        if (PR_UNSUCCESSFUL(ps = PR_parseEquals())) {
            t = &(ps -> token[0]);
            errorCode = PR_EXPECTING_EQUALS;
            goto pragmaError;
        }
        ps = PR_parseLiteral((char *) 0);
        t = &(ps -> token[0]);
        if (PR_UNSUCCESSFUL(ps)) {
            errorCode = PR_EXPECTING_LITERAL;
            goto pragmaError;
        }
        if ((literalPos -> page != 0) || (literalPos -> offset != 0)) {
            errorCode = PR_DUPLICATE_STATEMENT;
            goto pragmaError;
        }
        literalPos -> page = t -> value.valuePos.page;
        literalPos -> offset = t -> value.valuePos.offset;
    }
    return ps;

pragmaError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_GROUP_ERROR + errorCode));
    return ps;
}



static void PR_cClass(PR_Token_t *t, DMI_STRING  *class,
                      unsigned long groupId)
{
/*
    The syntax of a well-formed class string should be
    "<Company Name> | <Standard Name> | <version>"

    This procedure checks for the existence of each field (> 0 characters),
    and the vertical bar separators.

    In addition, if the group ID is 1, then this should be the component ID
    group.  That group should have the class string "DMTF|ComponentID|1.0".

    If the group ID is 0, then this is a template.  Templates are referenced by
    table blocks by the class string.  Thus, for templates, the class string
    must be unique.
*/
    PR_ErrorNumber_t errorCode;
    char             *classString;
    unsigned long    classLength;
    char             cidBody[]      = PR_COMPONENT_ID_GROUP_BODY;
    char             cidClassName[] = PR_COMPONENT_ID_GROUP_CLASS;
    char             *field;
    MIF_Bool_t       fieldExists;
    struct GroupData *g;
    char             *p;

    classString = (char *) 0;

    classLength = class -> length;
    if ((classString = (char *) OS_Alloc((size_t) (classLength + 1))) ==
        (char *) 0) {
        errorCode = PR_OUT_OF_MEMORY;
        goto classFatal;
    }
    strncpy(classString, class -> body, (size_t) classLength);
    classString[classLength] = '\0';

    errorCode = PR_INFO + PR_ILLEGAL_VALUE;
    fieldExists = MIF_FALSE;
    field = classString;
    for (p = classString; (*p != '\0') && (*p != '|'); ++p)
        fieldExists = MIF_TRUE;

/*
    Defining Body
*/

    if (! fieldExists)
        goto classError;

    if (*p != '|')
        goto classError;

    if ((groupId == PR_COMPONENT_ID_GROUP) &&
        (strncmp(field, cidBody, strlen(cidBody)) != 0)) {
        errorCode = PR_WARN + PR_CID_ERROR;
        goto classError;
    }

    field = ++p;
    fieldExists = MIF_FALSE;
    while ((*p != '\0') && (*p != '|')) {
        fieldExists = MIF_TRUE;
        ++p;
    }

/*
    Class Name
*/

    if (! fieldExists)
        goto classError;

    if (*p != '|')
        goto classError;

    if ((groupId == PR_COMPONENT_ID_GROUP) &&
        (strncmp(field, cidClassName, strlen(cidClassName)) != 0)) {
        errorCode = PR_WARN + PR_CID_ERROR;
        goto classError;
    }

/*
    Version
*/

    if (*p == '\0')
        goto classError;

    if ((groupId == 0) && (GroupData.next != (struct GroupData *) 0))
        for (g = GroupData.next;
             g != (struct GroupData *) 0;
             g = g -> next)
            if ((g -> pClass -> length == classLength) &&
                (strncmp(g -> pClass -> body, classString,
                         (size_t) classLength) == 0)) {
                PR_errorLogAdd(t -> line, t -> col, (PR_ErrorNumber_t)
                    (PR_ERROR + PR_TEMPLATE_ERROR + PR_CONFLICTING));
            }
         
    OS_Free(classString);
    return;

classFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_GROUP_ERROR + errorCode));

classError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_GROUP_ERROR + errorCode));
    OS_Free(classString);
}

static void PR_cGroup(PR_Token_t *t)
{
    PR_ErrorNumber_t errorCode;
    struct GroupData *g;

    for (g = GroupData.next; g -> next != (struct GroupData *) 0; g = g -> next)
        ;
    if (g == (struct GroupData *) 0)
        return;

    if (g -> pClass == (DMI_STRING *) 0) {
        errorCode = PR_MISSING_CLASS;
        goto groupError;
    }

    if (PR_attributeCount(g) == 0) {
        errorCode = PR_NO_ATTRIBUTES;
        goto groupError;
    }

    PR_cKey(t);
    return;

groupError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_GROUP_ERROR + errorCode));
    PR_dGroup();
}

static void PR_dGroup(void)
{
    struct GroupData *g;

    if (GroupData.next == (struct GroupData *) 0)
        return;
    for (g = &GroupData;
         g -> next -> next != (struct GroupData *) 0;
         g = g -> next)
        ;
    if (g -> next -> pGroupName != (DMI_STRING *) 0)
        OS_Free(g -> next -> pGroupName);
    if (g -> next -> pClass != (DMI_STRING *) 0)
        OS_Free(g -> next -> pClass);
    if (g -> next -> iKeyList != (unsigned long *) 0)
        OS_Free(g -> next -> iKeyList);
    OS_Free(g -> next);
    g -> next = (struct GroupData *) 0;
}

static void PR_eGroup(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;

    if (PR_SUCCESSFUL(ps = PR_parseKeyword(Ks.start))) {
        t = PR_tokenTableGetCurrent();
        errorCode = PR_EXPECTING_BLOCK;
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_STATEMENT;
    }
    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_GROUP_ERROR + errorCode));
    PR_skipToTokenSequence("KK", Ks.end, Ks.group);
    PR_dGroup();
}

static void PR_pKeyStmt(void)
{
    PR_ErrorNumber_t errorCode;
    PR_ParseStruct_t *ps;
    PR_Token_t       *t;
    struct GroupData *g;
    PR_Key_t         keyList;
    char             *literal;
    unsigned long    id;
    PR_Key_t         *key;
    PR_Key_t         *k;
    unsigned long    keyCount;
    int              i;

    if (PR_SUCCESSFUL(ps = PR_parseEquals())) {
        keyList.id = 0;
        keyList.next = (PR_Key_t *) 0;
        for (;;) {
            if (PR_SUCCESSFUL(ps = PR_parseNumber((char *) 0))) {
                t = &(ps -> token[0]);
                literal = PR_tokenLiteralGet(t);
                id = PR_atol(literal);
                if ((key = (PR_Key_t *) OS_Alloc(sizeof(PR_Key_t))) ==
                    (PR_Key_t *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto keyFatal;
                }
                key -> id = id;
                for (k = &keyList;k != (PR_Key_t *) 0;k = k -> next){  /* check for dupes */
                    if (id == k -> id) {
                        OS_Free(key);    /* free it up first */
                        PR_errorLogAdd(t -> line, t -> col,
                            (PR_ErrorNumber_t) (PR_ERROR + PR_KEY_ERROR + PR_CONFLICTING));
                        goto keyFinish;
                    }
                }
                /* now look for the insertion point for this sucker */
                for (k = &keyList;(k -> next != (PR_Key_t *) 0) && (id > k -> next -> id);k = k -> next);
                key -> next = k -> next;
                k -> next = key;
            } else {
                t = &(ps -> token[0]);
                errorCode = PR_EXPECTING_ID;
                goto keyError;
            }

            if (PR_UNSUCCESSFUL(PR_parseComma())) {
                if (keyList.next == (PR_Key_t *) 0) {
                    errorCode = PR_MISSING_VALUE;
                    goto keyError;
                }
                keyCount = 0;
                for (k = keyList.next; k != (PR_Key_t *) 0; k = k -> next)
                    ++keyCount;
                for (g = GroupData.next;
                     g -> next != (struct GroupData *) 0;
                     g = g -> next)
                    ;
                if(g -> iKeyList != (unsigned long *) 0){  /* we already have a key statement, error time */
                    PR_errorLogAdd(t -> line, t -> col,
                        (PR_ErrorNumber_t) (PR_ERROR + PR_GROUP_ERROR + PR_DUPLICATE_STATEMENT));
                    goto keyFinish;
                }
                if ((g -> iKeyList = (unsigned long *)
                     OS_Alloc((size_t) (keyCount * sizeof(unsigned long)))) ==
                    (unsigned long *) 0) {
                    errorCode = PR_OUT_OF_MEMORY;
                    goto keyFatal;
                }
                g -> iKeyCount = keyCount;
                i = 0;
                for (k = keyList.next, i = 0;
                     k != (PR_Key_t *) 0;
                     k = k -> next)
                    g -> iKeyList[i++] = k -> id;
                goto keyFinish;
            }
        }
    } else {
        t = &(ps -> token[0]);
        errorCode = PR_EXPECTING_EQUALS;
        goto keyError;
    }

keyError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + PR_KEY_ERROR + errorCode));

keyFatal:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_FATAL + PR_KEY_ERROR + errorCode));

keyFinish:

    while (keyList.next != (PR_Key_t *) 0) {
        for (k = &keyList; k -> next -> next != (PR_Key_t *) 0; k = k -> next)
            ;
        OS_Free(k -> next);
        k -> next = (PR_Key_t *) 0;
    }
    return;

}

static void PR_cKey(PR_Token_t *t)
{
    PR_ErrorNumber_t errorCode;
    struct GroupData *g;
    unsigned long    i;

    for (g = GroupData.next; g -> next != (struct GroupData *) 0; g = g -> next)
        ;

    if ((g -> iGroupId == 0) && (g -> iKeyCount == 0)) {
        errorCode = PR_TEMPLATE_ERROR + PR_NO_KEY;
        goto keyError;
    }
    for (i = 0; i < g -> iKeyCount; ++i)
        if (! PR_attributeExists(g, g -> iKeyList[i])) {
            errorCode = PR_KEY_ERROR + PR_ILLEGAL_VALUE;
            goto keyError;
        }
    return;

keyError:

    PR_errorLogAdd(t -> line, t -> col,
        (PR_ErrorNumber_t) (PR_ERROR + errorCode));
    PR_dGroup();
}

void *PR_groupClassToHandle(DMI_STRING *class)
{
    struct GroupData *g;

    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (PR_stringCmp(g -> pClass, class) == 0)
            return g;

    return 0;
}

void *PR_groupCopy(unsigned long targetId, void *groupHandle)
{
    struct GroupData *g;
    struct GroupData *t;
    unsigned long    size;

    g = (struct GroupData *) groupHandle;

    for (t = GroupData.next; t -> next != (struct GroupData *) 0; t = t -> next)
        ;
    if ((t -> next = (struct GroupData *) OS_Alloc(sizeof(struct GroupData))) ==
        (struct GroupData *) 0)
        goto groupFatal;
    t = t -> next;
    t -> next = (struct GroupData *) 0;

    size = PR_stringSize(g -> pGroupName);
    if ((t -> pGroupName = (DMI_STRING *) OS_Alloc((size_t) size)) ==
        (DMI_STRING *) 0)
        goto groupFatal;
    memcpy(t -> pGroupName, g -> pGroupName, (size_t) size);
    size = PR_stringSize(g -> pClass);
    if ((t -> pClass = (DMI_STRING *) OS_Alloc((size_t) size)) ==
        (DMI_STRING *) 0)
        goto groupFatal;
    memcpy(t -> pClass, g -> pClass, (size_t) size);

    t -> descriptionPos.page = g -> descriptionPos.page;
    t -> descriptionPos.offset = g -> descriptionPos.offset;

    size = sizeof(unsigned long) * g -> iKeyCount;
    if ((t -> iKeyList = (unsigned long *) OS_Alloc((size_t) size)) ==
        (unsigned long *) 0)
        goto groupFatal;
    memcpy(t -> iKeyList, g -> iKeyList, (size_t) size);
    t -> iGroupId = targetId;
    t -> iKeyCount = g -> iKeyCount;

    t -> pragmaPos.page = g -> pragmaPos.page;
    t -> pragmaPos.offset = g -> pragmaPos.offset;

    return t;

groupFatal:

    PR_errorLogAdd(0, 0, PR_FATAL + PR_GROUP_ERROR + PR_OUT_OF_MEMORY);
}

DMI_STRING *PR_groupClass(void *groupHandle)
{
    return ((struct GroupData *) groupHandle) -> pClass;
}

unsigned long PR_groupCount(void)
{
    struct GroupData *g;
    unsigned long    groupCount;

    groupCount = 0;
    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (g -> iGroupId != 0)
            ++groupCount;

    return groupCount;
}

DMI_STRING *PR_groupDescription(void *groupHandle)
{
    if (groupHandle == (void *) 0)
        return (DMI_STRING *) 0;

    return PR_tokenTableStringGet(((struct GroupData *) groupHandle) ->
                                  descriptionPos);
}

DMI_STRING *PR_groupPragma(void *groupHandle)
{
    if (groupHandle == (void *) 0)
        return (DMI_STRING *) 0;

    return PR_tokenTableStringGet(((struct GroupData *) groupHandle) ->
                                  pragmaPos);
}

MIF_Bool_t PR_groupExists(unsigned long groupId)
{
    struct GroupData *g;

    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (g -> iGroupId == groupId)
            return MIF_TRUE;

    return MIF_FALSE;
}

unsigned long PR_groupHandleToId(void *groupHandle)
{
    if (groupHandle == (void *) 0)
        return 0;
    else
        return ((struct GroupData *) groupHandle) -> iGroupId;
}

unsigned long PR_groupId(void *groupHandle)
{
    return ((struct GroupData *) groupHandle) -> iGroupId;
}

void PR_groupIdStore(void *groupHandle, unsigned long groupId)
{
    ((struct GroupData *) groupHandle) -> iGroupId = groupId;
}

void *PR_groupIdToHandle(unsigned long groupId)
{
    struct GroupData *g;

    if (groupId == 0)
        return (void *) 0;

    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (g -> iGroupId == groupId)
            return g;

    return (void *) 0;
}

unsigned long PR_groupKeyCount(void *groupHandle)
{
    return ((struct GroupData *) groupHandle) -> iKeyCount;
}

MIF_Bool_t PR_groupKeyExists(void *groupHandle, unsigned long attributeId)
{
    struct GroupData *g;
    unsigned long    i;

    g = (struct GroupData *) groupHandle;
    for (i = 0; i < g -> iKeyCount; ++i)
        if (g -> iKeyList[i] == attributeId)
            return MIF_TRUE;

    return MIF_FALSE;
}

unsigned long PR_groupKeyNext(void *groupHandle, unsigned long attributeId)
{
    struct GroupData *g;
    unsigned long    currentId;
    unsigned long    foundId;
    unsigned long    i;

    g = (struct GroupData *) groupHandle;
    foundId = 0;
    for (i = 0; i < g -> iKeyCount; ++i) {
        currentId = g -> iKeyList[i];
        if (((foundId == 0) && (currentId > attributeId)) ||
            ((currentId < foundId) && (currentId > attributeId)))
            foundId = currentId;
    }
    return foundId;
}

DMI_STRING *PR_groupName(void *groupHandle)
{
    return ((struct GroupData *) groupHandle) -> pGroupName;
}

void PR_groupNameStore(void *groupHandle, DMI_STRING *groupName)
{
    struct GroupData *g;
    unsigned long    stringSize;

    g = (struct GroupData *) groupHandle;
    if (g -> pGroupName != (DMI_STRING *) 0)
        OS_Free(g -> pGroupName);
    stringSize = PR_stringSize(groupName);
    if ((g -> pGroupName = (DMI_STRING *) OS_Alloc((size_t) stringSize)) ==
        (DMI_STRING *) 0)
        PR_errorLogAdd(0, 0, PR_FATAL + PR_GROUP_ERROR +
                       PR_OUT_OF_MEMORY);
    memcpy(g -> pGroupName, groupName, (size_t) stringSize);
}

unsigned long PR_groupNameToId(DMI_STRING *groupName)
{
    struct GroupData *g;

    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (PR_stringCmp(g -> pGroupName, groupName) == 0)
            return g -> iGroupId;

    return 0;
}

unsigned long PR_groupNext(void *groupHandle)
{
    struct GroupData *g;
    unsigned long groupId;
    unsigned long currentId;
    unsigned long foundId;

    if (groupHandle != (void *) 0)
        groupId = ((struct GroupData *) groupHandle) -> iGroupId;
    else
        groupId = 0;
    foundId = 0;
    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next) {
        currentId = g -> iGroupId;
        if (((foundId == 0) && (currentId > groupId)) ||
            ((currentId > groupId) && (currentId < foundId)))
            foundId = currentId;
    }
    return foundId;
}

void *PR_groupTemplateHandle(void *groupHandle)
{
    if (groupHandle == (void *) 0)
        return (void *) 0;

    return ((struct GroupData *) groupHandle) -> pTemplateHandle;
}

void PR_groupTemplateHandleStore(void *groupHandle, void *templateHandle)
{
    if (groupHandle == (void *) 0)
        return;

    ((struct GroupData *) groupHandle) -> pTemplateHandle = templateHandle;
}

unsigned long PR_unusedTemplateCount(void)
{
    struct GroupData *g;
    unsigned long    templateCount;

    templateCount = 0;
    for (g = GroupData.next; g != (struct GroupData *) 0; g = g -> next)
        if (g -> iGroupId == 0)
            ++templateCount;

    return templateCount;
}
