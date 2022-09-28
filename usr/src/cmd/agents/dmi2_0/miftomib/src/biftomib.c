/* Copyright 03/21/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)biftomib.c	1.7 97/03/21 Sun Microsystems"
#else
static char sccsid[] = "@(#)biftomib.c	1.7 97/03/21 Sun Microsystems";
#endif
#endif

/**********************************************************************
    Filename: biftomib.c

    Copyright (c) Intel, Inc. 1992,1993,1994

    Description: BIF to MIB translator

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/bif/rcs/biftomib.c 1.3 1994/11/18 07:04:38 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        8/10/94  aip    Creation date.
        11/14/94 aip    Revised.
        11/18/94 aip    Fixed t object.
	8/16/96  jwy    change DISPLAYSTRING DisplayString
	1/14/97  jwy	bug 4019329

/************************ INCLUDES ***********************************/

#include <ctype.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "biftomib.h"
#include "common.h"	/* For definition of MIF_COUNTER */
#include "os_dmi.h"
#include "pr_comp.h" /* $MED */
#include "pr_group.h" /* $MED */
#include "pr_attr.h" /* $MED */
#include "pr_enum.h" /* $MED */
#include "pr_main.h" /* $MED */
#include "pr_todmi.h" /* $MED */
#include "pr_lex.h" /* $MED */

/*********************************************************************/

/************************ PRIVATE ************************************/

static void translateComponent(void);
static void translateGroup(unsigned long); /* $MED */
static void translateAttribute(void *GroupHandle,
                               unsigned long GroupID,
                               unsigned long AttrID);
static void translateDescription(DMI_STRING *); /* $MED */

static void writeBeginning(char *enterpriseName,
                           char *enterpriseValue); /* $MED */
static void writeEnding(void);

static void mibOpen(char *mibPathname);
static void mibWrite(char *format, ...);
static void mibWriteLine(char *format, ...);
static void mibClose(void);
static void mibDelete(char *mibPathname);

static void dictionaryInit(void);
static char *dictionaryAddBifString(DMI_STRING *bifString);
static char *dictionaryLookUpMibString(DMI_STRING *bifString);

static char *bifComponentNameToMibIdent(DMI_STRING *componentName);
static char *bifAttributeIdToMibIdent(void *GroupHandle,
                                      unsigned long GroupID,
                                      unsigned long AttrID);
static char *bifAttributeAccessToMibAccess(unsigned long bifAttributeAccess);
static char *bifAttributeTypeToMibType(unsigned long bifAttributeType);
static char *prependMibIdent(char *prependString, char *mibIdent);

static void fatal(char *format, ...);

/*********************************************************************/

/************************ MACROS *************************************/

#define CHECK_PTR(ptr) \
    if ((void *) (ptr) == (void *) 0) \
        fatal("Failure while reading BIF.");

/*********************************************************************/

/************************ GLOBALS ************************************/

#ifdef STAND_ALONE

    static FILE *BIF;

#else

    extern FILE *BIF;

#endif

static struct Dictionary {
    struct Dictionary *next;
    DMI_STRING *bifString;
    char              *mibString;
    unsigned long     collisionCount;
} Dictionary;

static FILE    *MIB;
static jmp_buf BifToMibException;

/*********************************************************************/

/************************ DEFINES ************************************/

#define DMI_LAST_DATA_TYPE     MIF_DATE
#define MIB_IDENT_MAX          40
#define MIB_OID_IDENT_MAX      20
#define ILLEGAL_MIB_CHARACTERS "()/,;."

/*********************************************************************/

BifToMibStatus_t bifToMib(char *mibPathname,
                          char *enterpriseName,
                          char *enterpriseValue) /* $MED */
{
    if (setjmp(BifToMibException) != 0) {
        mibDelete(mibPathname);
        return BifToMibFailure;
    }

    dictionaryInit();
    mibOpen(mibPathname);
    writeBeginning(enterpriseName, enterpriseValue); /* $MED */
    translateComponent();
    writeEnding();
    mibClose();
    return BifToMibSuccess;
}

static void translateComponent(void)
{
    void             *GroupHandle;
    unsigned long    GroupID;   /* used to walk through the PR_groupNext() call */

/*    // need check for valid handle? $MED*/

    GroupID = PR_groupNext(0);   /* get the first group in the component */
    while(GroupID != 0){   /* walk through the group table */
        GroupHandle = (void *)PR_groupIdToHandle(GroupID);
        translateGroup(GroupID);
        GroupID = PR_groupNext(GroupHandle);   /* get the first group in the component */
    }
}

static void translateGroup(unsigned long GroupID) /* $MED */
{
    struct BIF_Group     *bgPtr;
    DMI_STRING    *bifGroupName;
    char                 mibGroupName[MIB_IDENT_MAX + 1];
    struct BIF_Key       *bkPtr;
    unsigned long        keyListOffset;
    unsigned long        keyListIndex;
    struct BIF_Attribute *baPtr;
    unsigned long        attributeOffset;
    unsigned long        attributeIndex;

    void             *GroupHandle;
    unsigned long    AttrID;
    unsigned long    fFirstAttr = 1;
    unsigned long    keyCount;

/*    // need check for valid handle? $MED*/
    GroupHandle = (void *)PR_groupIdToHandle(GroupID);

/*    // do i need to check or validate grouphandle? (nonnull, etc) $MED*/

    bifGroupName = PR_groupName(GroupHandle);
/*    // validate this? $MED*/
    strcpy(mibGroupName, dictionaryAddBifString(bifGroupName));

/*
    Create MIB sequence type.
*/

    mibWriteLine("");
    mibWriteLine("%s ::= SEQUENCE {", prependMibIdent("S", mibGroupName));

    AttrID = PR_attributeNext(GroupHandle,0);
    while(AttrID != 0){
        if (fFirstAttr) /*// wrote an attribute already... $MED*/
           fFirstAttr = 0;
        else
            mibWriteLine(",");
        mibWrite("\t%-*s\t%s", MIB_IDENT_MAX,
           bifAttributeIdToMibIdent(GroupHandle, GroupID, AttrID),
           bifAttributeTypeToMibType(PR_attributeType(GroupHandle,AttrID)));
        AttrID = PR_attributeNext(GroupHandle,AttrID);
    }

    mibWriteLine("");
    mibWriteLine("}");

/*
    Create MIB table object type.
*/

    mibWriteLine("");
    mibWriteLine("%s OBJECT-TYPE", prependMibIdent("t", mibGroupName));
    mibWriteLine("\tSYNTAX\t\tSEQUENCE OF %s",
                 prependMibIdent("S", mibGroupName));
    mibWriteLine("\tACCESS\t\tnot-accessible");
    mibWriteLine("\tSTATUS\t\tmandatory");

/*    // is GroupHandle, GroupID still good here? $MED*/
    translateDescription(PR_groupDescription(GroupHandle));
    mibWriteLine("\t::= {dmtfGroups %ld}", GroupID);

/*
    Create MIB entry object type.
*/

    mibWriteLine("");
    mibWriteLine("%s OBJECT-TYPE", prependMibIdent("e", mibGroupName));
    mibWriteLine("\tSYNTAX\t\t%s", prependMibIdent("S", mibGroupName));
    mibWriteLine("\tACCESS\t\tnot-accessible");
    mibWriteLine("\tSTATUS\t\tmandatory");
    translateDescription((DMI_STRING *)NULL);
    mibWrite("\tINDEX\t\t{dmiComponentIndex");
/* (8-61-96)
    mibWrite("\tINDEX\t\t{INTEGER");
*/

    keyCount = PR_groupKeyCount(GroupHandle);
    if (keyCount > 0) {
        AttrID = PR_groupKeyNext(GroupHandle,0);  /* get the first attr in the key list */
        while(AttrID != 0){
            mibWrite(", %s", bifAttributeIdToMibIdent(GroupHandle, GroupID, AttrID));
            AttrID = PR_groupKeyNext(GroupHandle, AttrID);
        }
    }
    mibWriteLine("}");
    mibWriteLine("\t::= {%s 1}", prependMibIdent("t", mibGroupName));

/*
    Iterate on attributes, creating an object type for each.
*/

    AttrID = PR_attributeNext(GroupHandle,0);
    while(AttrID != 0){
        translateAttribute(GroupHandle,GroupID,AttrID);
        AttrID = PR_attributeNext(GroupHandle,AttrID);
    }
}

static void translateAttribute(void *GroupHandle,
                               unsigned long GroupID,
                               unsigned long AttrID)
{
    DMI_STRING      *groupName;
    char            *mibGroupName;
    DMI_STRING      *enumString;
    void            *EnumHandle;
    unsigned long   enumNumber,enumValue;
    unsigned long   enumCount;
    char            buffer[sizeof(enumString -> length) + PR_LITERAL_MAX];
    unsigned long   i;
    unsigned long   Access;

    CHECK_PTR (
        groupName = PR_groupName(GroupHandle));
    CHECK_PTR (
        mibGroupName = dictionaryLookUpMibString(groupName));

    mibWriteLine("");
    mibWriteLine("%s OBJECT-TYPE",
        bifAttributeIdToMibIdent(GroupHandle, GroupID, AttrID));

    EnumHandle = PR_attributeEnumHandle(GroupHandle,AttrID);

    if(EnumHandle != (void *)NULL) {
        mibWriteLine("\tSYNTAX\t\tINTEGER {");

        enumCount = PR_enumElementCount(EnumHandle);
        for (i = 1; i <= enumCount; ++i) {
            enumString = PR_enumElementIndexToName(EnumHandle, i);

            memset(buffer, 0x00, sizeof(buffer));   /* kpb */

/*            // $MED what are the next 2 statements for *here*/
            memcpy(buffer, enumString, (size_t) (sizeof(enumString -> length) +
                   enumString -> length));
            enumString = (DMI_STRING *) buffer;
            enumValue = PR_enumElementValue(EnumHandle, enumString);
            if (i > 1)
                mibWriteLine(",");
            mibWrite("\t\t%-*s\t(%ld)", MIB_IDENT_MAX,
            prependMibIdent("v", dictionaryAddBifString(enumString)),
            enumValue);
        }

        mibWriteLine("");
        mibWriteLine("\t}");
    } else
        mibWriteLine("\tSYNTAX\t\t%s",
                     bifAttributeTypeToMibType(PR_attributeType(GroupHandle,AttrID)));

/*//$M Access = (PR_attributeStorage(GroupHandle,AttrID) << 31) | PR_attributeAccess(GroupHandle,AttrID);*/
    Access = PR_attributeAccess(GroupHandle,AttrID);

    mibWriteLine("\tACCESS\t\t%s",
                 bifAttributeAccessToMibAccess(Access));
    mibWriteLine("\tSTATUS\t\tmandatory");

    translateDescription(PR_attributeDescription(GroupHandle,AttrID));
    mibWriteLine("\t::= {%s %ld}", prependMibIdent("e", mibGroupName),
                 AttrID);
}

static void translateDescription(DMI_STRING *description)
{
    unsigned short    columnNumber;
    unsigned long     i;

    if (description == NULL) {
        mibWriteLine("\tDESCRIPTION\t\"\"");
        return;
    }

    mibWrite("\tDESCRIPTION\t\"");
    columnNumber = 16;
    for (i = 0; i < description -> length; ++i)
        if ((columnNumber >= 80) ||
            ((columnNumber >= 72) && (description -> body[i] == ' '))) {
            mibWriteLine("");
            columnNumber = 0;
        } else {
            mibWrite("%c", description -> body[i]);
            ++columnNumber;
        }
    mibWriteLine("\"");
}

static void writeBeginning(char *enterpriseName, char *enterpriseValue) /* $MED */
{
    unsigned long        dmiDataTypes[] = {MIF_COUNTER, MIF_COUNTER64,
                                           MIF_GAUGE, MIF_INTEGER,
                                           MIF_INTEGER64, MIF_OCTETSTRING,
                                           MIF_DISPLAYSTRING, MIF_DATE};

    unsigned long        dmiDataTypeCount[DMI_LAST_DATA_TYPE + 1];

    void             *GroupHandle;
    unsigned long    GroupID;   /* used to walk through the PR_groupNext() call */
    unsigned long    AttrID;
    unsigned long    Type;

    unsigned long        attributeOffset;
    unsigned long        attributeIndex;
    DMI_STRING    *componentName;
    DMI_STRING    *componentDescription;
    unsigned short       columnNumber;
    unsigned long        i;
/*    // add back checks for non-null component name, descr $MED*/

/*
    Write component name.
*/

    componentName = PR_componentName();

/*  mibWriteLine("-- MIB generated by miftomib version " BIF_TO_MIB_VERSION); $MED */
    mibWriteLine("");
    mibWriteLine("%s-MIB DEFINITIONS ::= BEGIN",
                 bifComponentNameToMibIdent(componentName));

/*
    Write component description.
*/

    mibWriteLine("");

    componentDescription = PR_componentDescription();

    columnNumber = 80;
    for (i = 0; i < componentDescription -> length; ++i) {
        if ((columnNumber >= 80) || ((columnNumber >=72) &&
            (componentDescription -> body[i - 1] == ' '))) {
            mibWrite("\n -- "); /* $MED */
            columnNumber = 4;
        }
        mibWrite("%c", componentDescription -> body[i]);
        ++columnNumber;
    }
    mibWriteLine("");

/*
    Determine which BIF data types are used.
*/

    for (i = 0; i < sizeof(dmiDataTypes) / sizeof(unsigned long); ++i)
        dmiDataTypeCount[dmiDataTypes[i]] = 0;

    GroupID = PR_groupNext(0);   /* get the first group in the component */
    while(GroupID != 0){   /* walk through the group table */
        GroupHandle = (void *)PR_groupIdToHandle(GroupID);

        AttrID = PR_attributeNext(GroupHandle,0);
        while(AttrID != 0){
            Type = PR_attributeType(GroupHandle,AttrID);
            ++dmiDataTypeCount[Type];
            AttrID = PR_attributeNext(GroupHandle,AttrID);
        }

        GroupID = PR_groupNext(GroupHandle);   /* get the first group in the component */
    }

/*
    Write the import section.
*/

    mibWriteLine("");
    mibWriteLine("IMPORTS");
    mibWriteLine("    OBJECT-TYPE\n    FROM RFC-1212");
    mibWrite("    ");
    if (dmiDataTypeCount[MIF_COUNTER] > 0)
        mibWrite("Counter, ");
    if (dmiDataTypeCount[MIF_GAUGE] > 0)
        mibWrite("Gauge, ");
    mibWrite("enterprises\n    FROM RFC1155-SMI");
    if (dmiDataTypeCount[MIF_DISPLAYSTRING] > 0)
        mibWrite("\n    DisplayString\n    FROM RFC1213-MIB");
    mibWriteLine(";");

/*
    Write data type definitions.
*/

    mibWriteLine("");
    for (i = 0; i < sizeof(dmiDataTypes) / sizeof(unsigned long); ++i)
        if (dmiDataTypeCount[dmiDataTypes[i]] != 0)
            switch (dmiDataTypes[i]) {
                case MIF_COUNTER:
                    mibWriteLine("DmiCounter\t\t\t::= INTEGER (0..4294967295)");
                    break;
                case MIF_COUNTER64:
                    mibWriteLine(
                    "DmiCounter64\t\t::= INTEGER (0..18446744073709551615)");
                    break;
                case MIF_GAUGE:
                    mibWriteLine("DmiGauge\t\t\t::= Gauge");
                    break;
                case MIF_INTEGER:
                    mibWriteLine("DmiInteger\t\t\t::= INTEGER");
                    break;
                case MIF_INTEGER64:
                    mibWriteLine("DmiInteger64\t\t::= INTEGER "
                    "(-18446744073709551615..18446744073709551615)");
                    break;
                case MIF_OCTETSTRING:
                    mibWriteLine("DmiOctetstring\t\t::= OCTET STRING");
                    break;
                case MIF_DISPLAYSTRING:
                    mibWriteLine("DmiDisplaystring\t::= DisplayString");
                    break;
                case MIF_DATE:
                    mibWriteLine(
                        "DmiDate\t\t\t\t::= OCTET STRING (SIZE (28))");
                    break;
            }

/*
    Write OID definitions.
*/

    mibWriteLine("");
    mibWriteLine("%-*s\tOBJECT IDENTIFIER ::= %s",
                 MIB_OID_IDENT_MAX, enterpriseName, enterpriseValue); /* $MED */
/*  mibWriteLine("%-*s\tOBJECT IDENTIFIER ::= {%s 1}", MIB_OID_IDENT_MAX,
                 "dmtfStd", enterpriseName); $MED */
/*  mibWriteLine("%-*s\tOBJECT IDENTIFIER ::= {dmtfStd 1}",
                 MIB_OID_IDENT_MAX, "dmtfComponents"); $MED */
    mibWriteLine("%-*s\tOBJECT IDENTIFIER ::= {%s 1}",
                 MIB_OID_IDENT_MAX, "dmtfGroups", enterpriseName);

    mibWriteLine("");
    mibWriteLine("dmiComponentIndex\t OBJECT-TYPE");
    mibWriteLine("\tSYNTAX DmiInteger");
    mibWriteLine("\tACCESS read-write");
    mibWriteLine("\tSTATUS mandatory");
    mibWriteLine("\tDESCRIPTION \"Component index used in other table.\"");
    mibWriteLine("\t::={dmtfGroups 3}\n");
/* no use (8-16-96)
    mibWriteLine("DmiComponentIndex\t::= INTEGER");
*/

}

static void writeEnding(void)
{
    mibWriteLine("");
    mibWriteLine("END");
}

static void mibOpen(char *mibPathname)
{
    MIB = fopen(mibPathname, "w");
    if (MIB == (FILE *) 0)
        fatal("Cannot open %s for writing.", mibPathname);
}

static void mibWrite(char *format, ...)
{
    va_list argPtr;

    if ((format != (char *) 0) && (strlen(format) != 0)) {
        va_start(argPtr, format);
        vfprintf(MIB, format, argPtr);
        va_end(argPtr);
    }
}

static void mibWriteLine(char *format, ...)
{
    va_list argPtr;

    if ((format != (char *) 0) && (strlen(format) != 0)) {
        va_start(argPtr, format);
        vfprintf(MIB, format, argPtr);
        va_end(argPtr);
    }
    putc('\n', MIB);
}

static void mibClose(void)
{
    if (MIB == (FILE *) 0)
        return;

    fclose(MIB);
    MIB = (FILE *) 0;
}

static void mibDelete(char *mibPathname)
{
    mibClose();
    remove(mibPathname);
}

static void dictionaryInit(void)
{
    struct Dictionary *d;

    while (Dictionary.next != (struct Dictionary *) 0) {
        d = Dictionary.next;
        if (d -> bifString != (DMI_STRING *) 0)
            free(d -> bifString);
        if (d -> mibString != (char *) 0)
            free(d -> mibString);
        Dictionary.next = d -> next;
        free(d);
    }
}

static char *dictionaryAddBifString(DMI_STRING *bifString)
{
    enum {LowerCase, UpperCase} characterCase;
    struct Dictionary           *d;
    struct Dictionary           *p;
    size_t                      bifStringSize;
    char                        *mibString;
    static char                 buffer[MIB_IDENT_MAX + 1];
    unsigned long               collisions;
    unsigned long               i;
    unsigned long               j;

    mibString = dictionaryLookUpMibString(bifString);
    if (mibString != (char *) 0)
        return mibString;

/*
    Add new dictionary entry at end of list.
*/

    for (d = &Dictionary; d -> next != (struct Dictionary *) 0; d = d -> next)
        ;

    if ((d -> next = (struct Dictionary *) malloc(sizeof(struct Dictionary))) ==
        (struct Dictionary *) 0)
        goto dictionaryAddBifStringFatal;
    d = d -> next;
    d -> next = (struct Dictionary *) 0;

/*
    Copy BIF string into entry.
*/

    bifStringSize = (size_t) (sizeof(bifString -> length) +
                             bifString -> length);
    if ((d -> bifString = (DMI_STRING *) malloc(bifStringSize)) ==
        (DMI_STRING *) 0)
        goto dictionaryAddBifStringFatal;
    memcpy(d -> bifString, bifString, (size_t) bifStringSize);

/*
    Copy MIB string into entry.  Ignore illegal MIB characters and characters
    past maximum MIB identifier size.
*/

    mibString = buffer;
    characterCase = UpperCase;
    for (i = 0, j = 0; (i < bifString -> length) && (j < MIB_IDENT_MAX); ++i)
        if (strchr(ILLEGAL_MIB_CHARACTERS, bifString -> body[i]) == (char *) 0)
            if (bifString -> body[i] != ' ') {
                if ((characterCase == LowerCase) &&
                    isupper(bifString -> body[i]))
                    mibString[j++] = (char) tolower(bifString -> body[i]);
                else if ((characterCase == UpperCase) &&
                         islower(bifString -> body[i]))
                    mibString[j++] = (char) toupper(bifString -> body[i]);
                else
                    mibString[j++] = bifString -> body[i];
                characterCase = LowerCase;
             } else
                characterCase = UpperCase;
    mibString[j] = '\0';
    if ((d -> mibString = (char *) malloc(strlen(mibString) + 1)) == (char *) 0)
        goto dictionaryAddBifStringFatal;
    strcpy(d -> mibString, mibString);

/*
    Converting the BIF string to a MIB string may have caused a collision.
    Count the number of duplicate dictionary entries.
*/

    collisions = 0;
    for (p = Dictionary.next; p != (struct Dictionary *) 0; p = p -> next)
        if (strcmp(mibString, p -> mibString) == 0)
            ++collisions;
    d -> collisionCount = --collisions;
    if (collisions > 0)
        sprintf(buffer, "%.*s%d", MIB_IDENT_MAX - 2, d -> mibString,
                collisions);

    return buffer;

dictionaryAddBifStringFatal:

    fatal("Out of memory in dictionaryAddBifString()");
}

static char *dictionaryLookUpMibString(DMI_STRING *bifString)
{
    struct Dictionary *d;

    for (d = Dictionary.next; d != (struct Dictionary *) 0; d = d -> next)
        if ((d -> bifString -> length == bifString -> length) &&
            (strncmp(d -> bifString -> body, bifString -> body,
                     (size_t) bifString -> length) == 0))
            return d -> mibString;

    return (char *) 0;
}

static char *bifComponentNameToMibIdent(DMI_STRING *componentName)
{
    char   *mibIdent;
    size_t i;

    if (componentName == (DMI_STRING *) 0)
        return (char *) 0;

    mibIdent = dictionaryAddBifString(componentName);
    for (i = 0; i < strlen(mibIdent); ++i)
        if (islower(mibIdent[i]))
            mibIdent[i] = (char) toupper(mibIdent[i]);

    return mibIdent;
}

/*// change parms of everyone who calls $MED*/
static char *bifAttributeIdToMibIdent(void *GroupHandle,
                                      unsigned long GroupID,
                                      unsigned long AttrID)
{
    DMI_STRING    *attributeName;
    char           attributePrepend[40];

    CHECK_PTR (
        attributeName = PR_attributeName(GroupHandle,AttrID));
    sprintf(attributePrepend, "a%d", GroupID);
    return prependMibIdent(attributePrepend,
           dictionaryAddBifString(attributeName));
}

static char *bifAttributeAccessToMibAccess(unsigned long bifAttributeAccess)
{
    static char *mibAccessTable[] = {(char *) 0, "read-only", "read-write",
                              "write-only", "not-accessible"};

    if (bifAttributeAccess >= sizeof(mibAccessTable) / sizeof(char *))
        return (char *) 0;
    if(bifAttributeAccess >  4) 
	bifAttributeAccess = 4;

    return mibAccessTable[bifAttributeAccess];
}

static char *bifAttributeTypeToMibType(unsigned long bifAttributeType)
{
    char *mibTypeTable[] = {(char *) 0, "DmiCounter", "DmiCounter64",
                            "DmiGauge", (char *) 0, "DmiInteger",
                            "DmiInteger64", "DmiOctetstring",
                            "DmiDisplaystring", (char *) 0, (char *) 0,
                            "DmiDate"};

    if (bifAttributeType > sizeof(mibTypeTable) / sizeof(char *))
        return (char *) 0;

    return mibTypeTable[bifAttributeType];
}

static char *prependMibIdent(char *prependString, char *mibIdent)
{
    static char prependedMibIdent[MIB_IDENT_MAX + 1];

    sprintf(prependedMibIdent, "%s", prependString);
    strncat(prependedMibIdent, mibIdent, MIB_IDENT_MAX - strlen(prependString));
    return prependedMibIdent;
}

static void fatal(char *format, ...)
{
    va_list argPtr;

    va_start(argPtr, format);
    vfprintf(stderr, format, argPtr);
    putc('\n', stderr);
    va_end(argPtr);
    longjmp(BifToMibException, 1);
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

