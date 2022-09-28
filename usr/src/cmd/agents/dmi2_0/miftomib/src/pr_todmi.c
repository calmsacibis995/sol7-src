/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_todmi.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_todmi.c
    
    Copyright (C) International Business Machines, Corp. 1995 - 1996

    Description: This module provides the functions to take the output of the
                 parser, and add it to the Service Layer database.

    Author(s): Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        10/05/95 par    Creation date.
        01/24/96 par    Modifed to support version 1.1

************************* INCLUDES ***********************************/

#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mif_db.h"
#include "db_api.h"
#include "os_svc.h"
#include "pr_attr.h"
#include "pr_comp.h"
#include "pr_enum.h"
#include "pr_group.h"
#include "pr_lex.h"
#include "pr_main.h"
#include "pr_parse.h"
#include "pr_path.h"
#include "pr_table.h"
#include "pr_todmi.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define CHECK_PTR(ptr) \
    if ((void *) (ptr) == (void *) 0) \
        longjmp(ToDmiException, 1);

#define CHECK_POS(pos) \
    if ((pos.page == 0) && (pos.offset == 0)) \
        longjmp(ToDmiException, 1);

typedef enum {BifValueType, BifOverlaySymbolType} BifDataType_t;


/*********************************************************************/

/************************ GLOBALS ************************************/

extern MIF_FileHandle_t DB;

static jmp_buf ToDmiException;

static struct EnumList {
    struct EnumList *next;
    void            *enumHandle;
    unsigned long   enumNumber;
    unsigned long   referenceCount;
} EnumList;

/*********************************************************************/

/************************ PRIVATE ************************************/

static BifToDmiStatus_t enumListBuild(void);
static void             enumListDelete(void);
static unsigned long    enumReferenceCount(void *enumHandle,unsigned long *enumNumber);
static void             transferComponent(void);
static void             transferPath(MIF_Id_t componentId,unsigned long PathCount);
static void             transferGroups(MIF_Id_t componentId);
static void             transferAttributes(MIF_Id_t componentId, MIF_Id_t groupId);
static MIF_Pos_t        transferEnumeration(MIF_Id_t componentId, MIF_Id_t groupId,unsigned long AttrID);
static MIF_Pos_t        transferData(void *Data,MIF_Unsigned_t dataType, unsigned long maxSize,unsigned long ValueType);
static void             transferTable(MIF_Id_t componentId,unsigned long GroupID);

/*********************************************************************/


BifToDmiStatus_t PR_ToDmi(void)
{
    if (enumListBuild() == BifToDmiFailure)
        return BifToDmiFailure;
    if (setjmp(ToDmiException) != 0) {
        enumListDelete();
        return BifToDmiFailure;
    }
    transferComponent();
    enumListDelete();
    return BifToDmiSuccess;
}

void transferComponent(void)
{
unsigned long        componentId;
MIF_Pos_t            dcPos;
DB_Component_t       *dcPtr;
MIF_Pos_t            dDescPos;
unsigned long        i,count;
void                 *hold;

    /* Create a DMI component with a name */
    dcPos = DB_componentAdd((DB_String_t *)PR_componentName());
    CHECK_POS(dcPos);

    /* Add the description */
    hold = PR_componentDescription();
    if(hold == (void *)NULL){
        CHECK_PTR (dcPtr = MIF_resolve(DB, dcPos, MIF_READ));
    }
    else{
        dDescPos = DB_stringAdd((DB_String_t *)hold);
        CHECK_PTR (
            dcPtr = MIF_resolve(DB, dcPos, MIF_WRITE));
        dcPtr -> description.page = dDescPos.page;
        dcPtr -> description.offset = dDescPos.offset;
    } 
    componentId = dcPtr -> id;

    /* Add the paths */
    count = PR_pathCount();   /* get the number of path element we are looking for here */
    for (i = 1; i <= count; ++i) transferPath(componentId,i);

    /* Add the groups */
    transferGroups(componentId);
}

static void transferPath(MIF_Id_t componentId,unsigned long PathCount)
{
MIF_Pos_t              dpPos;
DB_ComponentPath_t     *dpPtr;
MIF_Pos_t              dPathPos;
unsigned long          i,count;
DMI_STRING             *Path;

    count = PR_pathEnvironmentCount(PathCount);
    for (i = 1; i <= count; ++i) {
        /* Create a DMI path with a name */
        dpPos = DB_componentPathAdd(componentId,(DB_String_t *) PR_pathSymbolName(PathCount));
        CHECK_POS(dpPos);
        /* Add the pathname, if Path == NULL, then this is a direct interface element */
        Path = PR_pathEnvironmentPathName(PathCount,i);
        if(Path != (DMI_STRING *)NULL){
            dPathPos = DB_stringAdd((DB_String_t *)Path);
            CHECK_POS(dPathPos);
        }
        /* Update the members of the DB_ComponentPath structure */
        CHECK_PTR (dpPtr = MIF_resolve(DB, dpPos, MIF_WRITE));
        dpPtr -> iEnvironmentId = (MIF_Environment_t) PR_pathEnvironmentId(PathCount,i);
        if (Path == (DMI_STRING *)NULL)
            dpPtr -> pathType = MIF_DIRECT_INTERFACE_PATH_TYPE;
        else {
            dpPtr -> pathType = MIF_OVERLAY_PATH_TYPE;
            dpPtr -> pathValue.page = dPathPos.page;
            dpPtr -> pathValue.offset = dPathPos.offset;
        }
    }
}

static void transferGroups(MIF_Id_t componentId)
{
MIF_Pos_t        dClassPos;
MIF_Pos_t        dDescPos;
MIF_Pos_t        dPragmaPos;
MIF_Pos_t        dgPos;
MIF_Pos_t        TemplateAttrPos;
DB_Group_t       *dgPtr,*Template;
MIF_Pos_t        dKeyPos;
unsigned long    i,Count;
DB_Key_t         *keyList;
unsigned long    GroupID;   /* used to walk through the PR_groupNext() call */
unsigned long    AttrID,TemplateID;
void             *GroupHandle;
DMI_STRING       *Hold;         /* used to hold string ptrs for validation */

    GroupID = PR_groupNext(0);   /* get the first group in the component */
    while(GroupID != 0){   /* walk through the group table */
        /* Create a group with a name */
        GroupHandle = (void *)PR_groupIdToHandle(GroupID);
        dgPos = DB_groupAdd(componentId, GroupID,(DB_String_t *)PR_groupName(GroupHandle));
        CHECK_POS(dgPos);
        dClassPos.page = 0;
        dClassPos.offset = 0;
        /* Add the class */
        Hold = (DMI_STRING *)PR_groupClass(GroupHandle);
        if (Hold != (DMI_STRING *)NULL) {
            dClassPos = DB_stringAdd((DB_String_t *)Hold);
            CHECK_POS(dClassPos);
        } 
        /* Add the description */
        dDescPos.page = 0;
        dDescPos.offset = 0;
        Hold = PR_groupDescription(GroupHandle);
        if (Hold != (DMI_STRING *)NULL) {
            dDescPos = DB_stringAdd((DB_String_t *)Hold);
            CHECK_POS(dDescPos);
        } 
        /* Add the pragma */
        dPragmaPos.page = 0;
        dPragmaPos.offset = 0;
        Hold = PR_groupPragma(GroupHandle);
        if (Hold != (DMI_STRING *)NULL) {
            dPragmaPos = DB_stringAdd((DB_String_t *)Hold);
            CHECK_POS(dPragmaPos);
        } 
        /* Add the key list */
        Count = PR_groupKeyCount(GroupHandle);
        if (Count > 0) {
            CHECK_PTR(keyList = OS_Alloc((size_t) (sizeof(DB_Key_t) +
                          (Count - 1) * sizeof(keyList -> keys))));
            keyList->keyCount = Count;    /* save the count information for the keys */
            AttrID = PR_groupKeyNext(GroupHandle,0);  /* get the first attr in the key list */
            i = 0;     /* index into the key list array */
            while(AttrID != 0){
                keyList -> keys[i] = AttrID;   /* move the attribute ID into the key list array */
                AttrID = PR_groupKeyNext(GroupHandle,keyList -> keys[i]);
                i++;    /* increment the index into the keylist array for next pass */
            }
            dKeyPos = DB_keyAdd(componentId, GroupID, keyList);
            OS_Free(keyList);
            CHECK_POS(dKeyPos);
        }
        /* Add the attributes */
        transferAttributes(componentId,GroupID);
        /* Add the table data */

        TemplateID = PR_groupHandleToId(PR_groupTemplateHandle(GroupHandle));
        if(TemplateID != GroupID){    /* OK, this is based on a template, we need the attr info */
            Template = MIF_groupGet(componentId,TemplateID);   /* load up the template group */
            if(Template != (DB_Group_t *)NULL){
                TemplateAttrPos.page = Template->attributeLink.page;
                TemplateAttrPos.offset =Template->attributeLink.offset;
            }
        }

        transferTable(componentId,GroupID);
        /* Update the members of the DB_Group structure */
        CHECK_PTR (
            dgPtr = MIF_resolve(DB, dgPos, MIF_WRITE));
        dgPtr -> classStr.page = dClassPos.page;
        dgPtr -> classStr.offset = dClassPos.offset;
        dgPtr -> description.page = dDescPos.page;
        dgPtr -> description.offset = dDescPos.offset;
        dgPtr -> pragma.page = dPragmaPos.page;
        dgPtr -> pragma.offset = dPragmaPos.offset;
        if((TemplateID != GroupID) && (Template != (DB_Group_t *)NULL)){    /* OK, this is based on a template, we need the attr info */
            dgPtr -> attributeLink.page = TemplateAttrPos.page;
            dgPtr -> attributeLink.offset = TemplateAttrPos.offset;
        }
        GroupID = PR_groupNext(GroupHandle);   /* get the first group in the component */

    }
}
static void transferAttributes(MIF_Id_t componentId,MIF_Id_t groupId)
{
MIF_Pos_t            daPos;
DB_Attribute_t       *daPtr;
MIF_Unsigned_t       dataType;
MIF_Pos_t            dDataPos;
MIF_Pos_t            dDescPos;
MIF_Pos_t            dEnumPos;
MIF_Unsigned_t       maxSize;
unsigned long        AttrID;
void                 *GroupHandle,*TemplateHandle;
DMI_STRING           *Hold;
char                 *Value;
unsigned long        Type,Access;

    GroupHandle = PR_groupIdToHandle(groupId);
    TemplateHandle = PR_groupTemplateHandle(GroupHandle);
    if(TemplateHandle != (void *)NULL) GroupHandle = TemplateHandle;
    AttrID = PR_attributeNext(GroupHandle,0);
    while(AttrID != 0){
        /* Add an attribute with a name */
        daPos = DB_attributeAdd(componentId, groupId,AttrID,
                (DB_String_t *) PR_attributeName(GroupHandle,AttrID));
        CHECK_POS(daPos);
        /* Add the description */
        Hold = PR_attributeDescription(GroupHandle,AttrID);
        dDescPos.page = 0;
        dDescPos.offset = 0;
        if (Hold != (DMI_STRING *)NULL) {
            dDescPos = DB_stringAdd((DB_String_t *) Hold);
            CHECK_POS(dDescPos);
        }
        /* Add the enumeration */
        dEnumPos = transferEnumeration(componentId, groupId,AttrID);
        /* Add the value */
        dataType = PR_attributeValueType(GroupHandle,AttrID);
        maxSize = PR_attributeMaxSize(GroupHandle,AttrID);
        Value = PR_attributeValue(GroupHandle,AttrID);
        Type = PR_attributeType(GroupHandle,AttrID);
        Access = (PR_attributeStorage(GroupHandle,AttrID) << 31) | PR_attributeAccess(GroupHandle,AttrID);
        if ((Access == BIF_UNSUPPORTED) || (Value == (void *) 0) || (Access == BIF_UNKNOWN))
            dDataPos = DB_dataAdd(dataType,0,maxSize,0,0,(void *)0);
        else dDataPos = transferData(Value, Type, maxSize,dataType);
        CHECK_POS(dDataPos);
        /* Update the members of the DB_Attribute structure */
        CHECK_PTR (
            daPtr = MIF_resolve(DB, daPos, MIF_WRITE));
        daPtr -> dataType = Type;
        daPtr -> accessType = Access;
        daPtr -> description.page = dDescPos.page;
        daPtr -> description.offset = dDescPos.offset;
        daPtr -> enumLink.page = dEnumPos.page;
        daPtr -> enumLink.offset = dEnumPos.offset;
        daPtr -> dataPos.page = dDataPos.page;
        daPtr -> dataPos.offset = dDataPos.offset;
        AttrID = PR_attributeNext(GroupHandle,AttrID);
    }
}

static void enumListDelete(void)
{
struct EnumList *e;

    while (EnumList.next != (struct EnumList *) 0) {
        e = EnumList.next;
        EnumList.next = e -> next;
        OS_Free(e);
    }
}

static unsigned long enumReferenceCount(void *enumHandle,unsigned long *enumNumber)
{
struct EnumList *e;

    (*enumNumber) = 0;   /* preset this, incase we don't find the sucker */
    for (e = EnumList.next;(e != (struct EnumList *) 0) && (enumHandle <= e -> enumHandle);e = e -> next)
        if (enumHandle == e->enumHandle){
            (*enumNumber) = e->enumNumber;    /* set the number into the var */
            return e -> referenceCount;       /* return the reference count information */
        }
    return 0;
}

static MIF_Pos_t transferEnumeration(MIF_Id_t componentId, MIF_Id_t groupId,unsigned long AttrID)
{
MIF_Pos_t       nullPos                   = {0, 0};
char            enumName[80];
MIF_Pos_t       dEnumPos;
DB_Enum_t       *dEnumPtr;
MIF_Pos_t       dElPos;
unsigned long   i,Count;
DB_uString      (string, 80);
void            *GroupHandle,*EnumHandle;
unsigned long   enumNumber,enumValue;
DMI_STRING      *elementName;
char            buffer[sizeof(elementName -> length) + PR_LITERAL_MAX];

    GroupHandle = PR_groupIdToHandle(groupId);
    EnumHandle = PR_attributeEnumHandle(GroupHandle,AttrID);
    if(EnumHandle == (void *)NULL) return nullPos;

    /* Create enumeration name */
    if (enumReferenceCount(EnumHandle,&enumNumber) > 1)
        sprintf(enumName, "Enum%lu(%lu)", enumNumber,componentId);
    else {
        sprintf(enumName, "Enum(%lu, %lu, %lu)", componentId, groupId,AttrID);
        componentId = 0;
    }
    DB_stringInit((DB_String_t *) string, enumName);
    /* Check for name in database */
    dEnumPos = DB_enumFind(componentId, (DB_String_t *) string);
    if ((dEnumPos.page != 0) || (dEnumPos.offset != 0))
        return dEnumPos;
    /* Create an enumeration in the database */
    dEnumPos = DB_enumAdd(componentId, (DB_String_t *) string);
    CHECK_POS(dEnumPos);
    CHECK_PTR (
        dEnumPtr = MIF_resolve(DB, dEnumPos, MIF_WRITE));
    dEnumPtr -> dataType = MIF_INTEGER;
    /* Add the elements */
    Count = PR_enumElementCount(EnumHandle);
    for (i = 1; i <= Count; ++i) {
        elementName = PR_enumElementIndexToName(EnumHandle, i);
        memcpy(buffer, elementName, (size_t) (sizeof(elementName -> length) +
               elementName -> length));
        elementName = (DMI_STRING *) buffer;
        enumValue = PR_enumElementValue(EnumHandle, elementName);
        dElPos = DB_enumElementAdd(dEnumPos,(DB_String_t *) elementName,&enumValue);
        CHECK_POS(dElPos);
    }
    return dEnumPos;
}

static MIF_Pos_t transferData(void *DataValue, MIF_Unsigned_t dataType,MIF_Unsigned_t maxSize,unsigned long ValueType)
{
MIF_Unsigned_t  currentSize;
DB_String_t     *dbStringValue;
MIF_Pos_t       dDataPos;
MIF_Int_t       *integerValue;
MIF_Unsigned_t  objectSize;
void            *value;
MIF_Unsigned_t  valueSize;

    if (ValueType == BifValueType) { /* Value of attribute is an actual value */
        switch(dataType){
            case MIF_OCTETSTRING: 
            case MIF_DISPLAYSTRING:  /* Value is a string type */
                dbStringValue = (DB_String_t *)DataValue;
                CHECK_PTR (dbStringValue);
                if (sizeof(unsigned long) + dbStringValue -> length > maxSize)
                    maxSize = sizeof(unsigned long) + dbStringValue -> length;
                objectSize = maxSize;
                currentSize = sizeof(unsigned long) + dbStringValue -> length;
                valueSize = DB_stringSize(dbStringValue);
                value = dbStringValue;
                break;
            case MIF_DATE: /* Value is a DMI date structure. */
                value = DataValue;
                objectSize = sizeof(MIF_Date_t);
                maxSize = sizeof(MIF_Date_t);
                currentSize = sizeof(MIF_Date_t);
                valueSize = sizeof(MIF_Date_t);
                break;
            case MIF_COUNTER64:
            case MIF_INTEGER64:  /* Value is a 64 bit numeric type */
                value = DataValue;
                objectSize = 8;
                maxSize = 8;
                currentSize = 8;
                valueSize = 8;
                break;
            default:  /* Value is a 32 bit numeric type */
                integerValue = (MIF_Int_t *)DataValue;
                objectSize = 4;
                maxSize = 4;
                currentSize = 4;
                valueSize = 4;
                value = integerValue;
                break;
        }
    } 
    else { /* Value of attribute is overlay symbol name */
        switch(dataType){
            case MIF_OCTETSTRING:
            case MIF_DISPLAYSTRING:
                currentSize = maxSize;
                break;
            case MIF_COUNTER64:
            case MIF_INTEGER64:
                maxSize = 8;
                currentSize = 8;
                break;
            case MIF_DATE:
                maxSize = 28;
                currentSize = 28;
                break;
            default:
                maxSize = 4;
                currentSize = 4;
                break;
        }
        dataType = MIF_COMPONENT_PATH_NAME;
        dbStringValue = (DB_String_t *)DataValue;
        CHECK_PTR (dbStringValue);
        objectSize = DB_stringSize(dbStringValue);
        valueSize = objectSize;
        value = dbStringValue;
    }
    /* Create data object */
    dDataPos = DB_dataAdd(dataType, objectSize, maxSize, currentSize,valueSize, value);
    CHECK_POS(dDataPos);
    return dDataPos;
}

static void transferTable(MIF_Id_t componentId,unsigned long GroupID)
{
MIF_Pos_t            dTablePos;
MIF_Unsigned_t       columnNumber;
MIF_Unsigned_t       rowNumber;
MIF_Unsigned_t       dataType;
MIF_Unsigned_t       maxSize;
MIF_Pos_t            dDataPos;
MIF_Pos_t            dElPos;
unsigned long        a,Count;
unsigned long        i,AttrID;
void                 *GroupHandle,*TemplateHandle;
char                 *Value = NULL;

    if ((Count = PR_tableElementCount(GroupID)) == 0) return;  /* nothing in this table... */
    GroupHandle = PR_groupIdToHandle(GroupID);   /* grab the handle so we've got it for later */
    TemplateHandle = PR_groupTemplateHandle(GroupHandle);   /* grab the template handle */

    /* Create the table with a name */
    dTablePos = DB_tableAdd(componentId,GroupID,(DB_String_t *)PR_groupName(GroupHandle));
    CHECK_POS(dTablePos);
    rowNumber = 0;
    columnNumber = ULONG_MAX;
    for (i = 1; i <= Count; ++i) {    /* walk through the table elements here */
        AttrID = PR_tableElementId(GroupID,i);    /* get the attribute ID first */
        /* Adjust row and column */
        if (AttrID <= columnNumber) ++rowNumber;
        columnNumber = AttrID;
        /* Find attribute's type and maxSize in BIF */
        dataType = 0;
        maxSize = 0;
        for (a = 1; a <= Count; ++a) {
            AttrID = PR_tableElementId(GroupID,a);    /* get the attribute ID first */
            if (AttrID == columnNumber) {
                dataType = PR_attributeType(TemplateHandle,AttrID);
                maxSize  = PR_attributeMaxSize(TemplateHandle,AttrID);
                Value = PR_tableElementValue(GroupID,i);
                break;
            }
        }
        /* Create the data object */
        dDataPos = transferData(Value, dataType, maxSize,PR_attributeValueType(TemplateHandle,AttrID));
        /* Create the table element */
        dElPos = DB_tableElementAdd(dTablePos, columnNumber, rowNumber,dDataPos);
        CHECK_POS(dElPos);
    }
}

static BifToDmiStatus_t enumListBuild(void)
{
unsigned long        enumNumber,Count,AttrCount;
unsigned long        groupIndex;
unsigned long        attributeIndex;
struct EnumList      *e;
struct EnumList      *n;
void                 *GroupHandle,*enumHandle;

    enumNumber = 0;
    Count = PR_groupCount();
    for (groupIndex = 0; groupIndex < Count; ++groupIndex) {
        GroupHandle = PR_groupIdToHandle(groupIndex);   /* grab the handle so we've got it for later */
        AttrCount = PR_attributeCount(GroupHandle);
        for (attributeIndex = 0;attributeIndex < AttrCount;++attributeIndex) {
            enumHandle = PR_attributeEnumHandle(GroupHandle,attributeIndex);
            if(enumHandle != (void *)NULL){  /* we've got one, look in the enum table to see if it's there */
                for (e = EnumList.next; (e != (struct EnumList *) 0) &&
                     (e -> enumHandle != enumHandle); e = e -> next);
                if (e == (struct EnumList *) 0) {  /* Add new element to enumeration list. */
                    for (e = &EnumList;(e -> next != (struct EnumList *) 0) &&
                         (enumHandle > e -> next -> enumHandle);e = e -> next);
                    if ((n = (struct EnumList *)
                        OS_Alloc(sizeof(struct EnumList))) ==
                        (struct EnumList *) 0)
                        return BifToDmiFailure; 
                    n -> next = e -> next;
                    n -> enumHandle = enumHandle;
                    n -> enumNumber = ++enumNumber;
                    n -> referenceCount = 1;
                    e -> next = n;
                }
                else ++ (e -> referenceCount); /* Update element in list. */
            }
        }
    }
    return BifToDmiSuccess;
}

BIF_OsEnvironment_t BIF_stringToEnvironmentId(DMI_STRING *environment)
{
    char        *environments[] = {"", "DOS", "MACOS", "OS2", "UNIX", "WIN16",
                                   "WIN32", (char *) 0};
    PR_uString  (string, 80);
    int         i;

    for (i = 1; environments[i] != (char *) 0; ++i) {
        PR_stringInit((DMI_STRING *) string, environments[i]);
        if (PR_stringCmp((DMI_STRING *) string, environment) == 0)
            return (BIF_OsEnvironment_t) i;
    }

    return (BIF_OsEnvironment_t) 0;
}

BIF_AttributeDataType_t BIF_stringToType(DMI_STRING *string)
{
    PR_uString (s, 80);

    if ((PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                            "Counter")) == 0) ||
        (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                             "Counter32")) == 0))
        return BIF_COUNTER;
    else if (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                "Counter64")) == 0)
        return BIF_COUNTER64;
    else if (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                "Gauge")) == 0)
        return BIF_GAUGE;
    else if ((PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                 "Int")) == 0) ||
             (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                 "Integer")) == 0))
        return BIF_INT;
    else if (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                "Octetstring")) == 0)
        return BIF_OCTETSTRING;
    else if ((PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                 "DisplayString")) == 0) ||
             (PR_stringCmp(string, PR_stringInit((DMI_STRING *) s,
                                                 "String")) == 0))
        return BIF_DISPLAYSTRING;
    else
        return BIF_UNKNOWN_DATA_TYPE;
}

