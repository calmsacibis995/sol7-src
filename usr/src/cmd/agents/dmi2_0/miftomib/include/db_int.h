/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_int.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************

    Filename: db_int.h
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines Header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/db/rcs/db_int.h 1.40 1994/04/21 16:51:02 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
		05/06/93 sfh	Add key definitions.
        5/14/93  aip    Added description fields to objects.
		05/18/93 sfh	Revise to use MIF_String_t instead of char[]
						for strings;
        5/25/93  aip    Moved things around a bit.
                        Added Enumeration prototypes.
        5/26/93  aip    Added member to Attribute for anonymous enumerations.
                        Added elementCount member to DB_Enum_t
		05/28/93 sfh	Add stringCat.
        6/7/93   aip    Static Tables.
        6/22/93  aip    Added object copy prototypes.
		06/24/93 sfh	Added tableElementRowFind & tableElementColFind
						prototypes.
        6/25/93  aip    Changed data object.
		07/20/93 sfh	Changed accessType and dataType in DB_Attribute_t to
						MIF_Int_t.
		07/20/93 sfh	Change type of parameter for dataTypewSizeGet to
                        MIF_Int_t.
		07/26/93 sfh	Include dos_mi.h
        7/27/93  aip    Added definitions that were removed from old mi.h.
        7/28/93  aip    Using new dmi.h
                        Added MIF_Access_t definition.
                        Changed definitions to DMI_UNSIGNED.
		07/29/93 sfh	Move definition of MIF_String_t from dos_dmi.h.
        7/29/93  aip    Introduced MIF_Unsigned_t where appropriate.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
                        Changed arguments of string functions to _FAR for
                        Windows Service Layer.
		10/28/93 sfh	Change dos_dmi.h to os_dmi.h.
		11/30/93 sfh	Revise for asm version.
        12/6/93  aip    Added pathType member to DB_ComponentPath_t structure.
                        Changed path block.
        12/7/93  aip    Added MIF_MaHandle_t definition for component locking.
		12/14/93 sfh	Make SL procedures _PASCAL linkage.
		12/21/93 sfh	Remove _PASCAL.
		03/24/94 sfh	Make _FAR empty if not building service layer.
        4/6/94   aip    Added 32-bit string support.
        4/7/94   aip    Changed path structure to use environment ID, rather
                        than an offset to an environment string.
		04/08/94 sfh	Add MIF_NUM_ENVIRONMENTS to MIF_Environment_t.
        4/21/94  aip    Added MIF_TimeStamp_t
        10/28/94 aip    Removed prototype for DB_tableColumnFind().

**********************************************************************/

#ifndef DB_INT_H_FILE
#define DB_INT_H_FILE

/************************ INCLUDES ***********************************/

#include <time.h>
#include "mif_db.h"
#include "os_dmi.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define MIF_COMPONENT_PATH_NAME 0xFFFF0001L
#define DB_NAME_MAX 80

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
#ifdef __cplusplus
extern "C"
{
#endif

/* If not building service layer, all data is near                           */
#ifndef SL_BUILD
#ifdef _FAR
#undef _FAR
#define _FAR
#endif	/* _FAR                                                               */
#endif 	/* SL_BUILD                                                          */

typedef enum {MIF_UNKNOWN_TYPE, MIF_ATTRIBUTE, MIF_COMPONENT,
              MIF_COMPONENT_PATH, MIF_ENUM, MIF_GROUP, MIF_TABLE} MIF_Type_t;

typedef enum {MIF_UNKNOWN_PATH_TYPE, MIF_OVERLAY_PATH_TYPE,
              MIF_DIRECT_INTERFACE_PATH_TYPE} MIF_PathType_t;

typedef enum {MIF_UNKNOWN_ENVIRONMENT, MIF_DOS_ENVIRONMENT,
              MIF_MAC_OS_ENVIRONMENT, MIF_OS2_ENVIRONMENT, MIF_UNIX_ENVIRONMENT,
              MIF_WIN16_ENVIRONMENT, MIF_WIN32_ENVIRONMENT, 
              MIF_NUM_ENVIRONMENTS = MIF_WIN32_ENVIRONMENT} MIF_Environment_t;

typedef DMI_UNSIGNED    MIF_AccessType_t;
typedef DMI_UNSIGNED    MIF_DataType_t;
typedef DMI_TimeStamp_t MIF_Date_t;
typedef DMI_UNSIGNED    MIF_Id_t;
typedef DMI_INT         MIF_Int_t;
typedef DMI_UNSIGNED    MIF_MaHandle_t;
typedef DMI_UNSIGNED    MIF_Unsigned_t;

typedef struct {
            unsigned long length;
			char          body[1];
        } DB_String_t;

typedef struct {
            MIF_Type_t type;
            MIF_Pos_t  name;
            MIF_Pos_t  nameLinkNext;
        } DB_Name_t;

typedef struct {
            DB_Name_t         name;
            MIF_PathType_t    pathType;
            MIF_Environment_t iEnvironmentId;
            MIF_Pos_t         pathValue;
            MIF_Pos_t         pathLinkNext;
        } DB_ComponentPath_t;

typedef struct {
            DB_Name_t name;
            MIF_Id_t  id;
            MIF_Pos_t description;
            MIF_Pos_t pathLink;
            MIF_Pos_t enumLink;
            MIF_Pos_t tableLink;
            MIF_Pos_t groupLink;
        } DB_Component_t;

typedef struct {
            DB_Name_t      name;
            MIF_DataType_t dataType;
            MIF_Unsigned_t elementCount;
            MIF_Pos_t      elementLink;
            MIF_Pos_t      enumLinkNext;
        } DB_Enum_t;

typedef struct {
            MIF_Pos_t symbolPos;
            MIF_Pos_t dataPos;
            MIF_Pos_t elementLinkNext;
        } DB_EnumElement_t;

typedef struct {
            DB_Name_t name;
            MIF_Id_t       id;
            MIF_Unsigned_t columnCount;
            MIF_Unsigned_t rowCount;
            MIF_Pos_t      columnLink;
            MIF_Pos_t      rowLink;
            MIF_Pos_t      tableLinkNext;
        } DB_Table_t;

typedef struct {
            MIF_Unsigned_t number;
            MIF_Pos_t      link;
            MIF_Pos_t      linkNext;
        } DB_TableHeader_t;

typedef struct {
            MIF_Unsigned_t columnNumber;
            MIF_Unsigned_t rowNumber;
            MIF_Pos_t      dataPos;
            MIF_Pos_t      columnLinkNext;
            MIF_Pos_t      rowLinkNext;
        } DB_TableElement_t;

typedef struct {
            DB_Name_t name;
            MIF_Id_t  id;
            MIF_Pos_t classStr;
            MIF_Pos_t key;
            MIF_Pos_t description;
            MIF_Pos_t pragma;
            MIF_Pos_t groupLinkNext;
            MIF_Pos_t attributeLink;
            MIF_Pos_t tableLink;
        } DB_Group_t;

typedef struct {
	        MIF_Unsigned_t keyCount;
	        MIF_Id_t       keys[1];
        } DB_Key_t;

typedef struct {
            DB_Name_t      name;
            MIF_Id_t       id;
            MIF_Unsigned_t accessType;
            MIF_Unsigned_t dataType;
            MIF_Pos_t      enumLink;
            MIF_Pos_t      dataPos;
            MIF_Pos_t      description;
            MIF_Pos_t      attributeLinkNext;
        } DB_Attribute_t;

/*
    For a description of the diffrent size fields in the DB_Data_t object,
    refer to db_data.c
*/

typedef struct {
            MIF_DataType_t dataType;
            MIF_Unsigned_t objectSize;
            MIF_Unsigned_t maxSize;
            MIF_Unsigned_t currentSize;
            MIF_Pos_t      valuePos;
        } DB_Data_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

/* Attribute Procedures                                                      */

MIF_Pos_t    DB_attributeAdd(MIF_Id_t componentId, MIF_Id_t groupId,
                             MIF_Id_t attributeId, DB_String_t *name);
MIF_Status_t DB_attributeDelete(MIF_Id_t componentId, MIF_Id_t groupId,
                                MIF_Pos_t attributePos);
MIF_Pos_t 	 DB_attributeFindFirst(MIF_Id_t componentId, MIF_Id_t groupId);
MIF_Pos_t    DB_attributeFindNext(MIF_Pos_t attributePos);
MIF_Pos_t 	 DB_attributeIdToPos(MIF_Id_t componentId, MIF_Id_t groupId,
                                 MIF_Id_t attributeId);
MIF_Id_t     DB_attributeInsertionPointFind(MIF_Id_t componentId,
                                            MIF_Id_t groupId,
                                            MIF_Id_t attributeId);

/* Component Procedures                                                      */

MIF_Pos_t    DB_componentAdd(DB_String_t *name);
MIF_Status_t DB_componentDelete(MIF_Pos_t componentPos);
MIF_Pos_t    DB_componentIdToPos(MIF_Id_t id);
MIF_Id_t     DB_componentPosToId(MIF_Pos_t componentPos);

MIF_Pos_t    DB_componentPathAdd(MIF_Id_t componentId, DB_String_t *name);
MIF_Status_t DB_componentPathDelete(MIF_Id_t componentId,
                                    MIF_Pos_t componentPathPos);
MIF_Pos_t    DB_componentPathFindFirst(MIF_Id_t componentId);
MIF_Pos_t    DB_componentPathFindNext(MIF_Pos_t componentPathPos);

/* Data Procedures                                                           */

MIF_Pos_t    DB_dataAdd(MIF_DataType_t type, MIF_Unsigned_t objectSize,
                        MIF_Unsigned_t maxSize, MIF_Unsigned_t currentSize,
                        MIF_Unsigned_t valueSize, void *value);
MIF_Status_t DB_dataDelete(MIF_Pos_t dataPos);
short 		 DB_dataTypeSizeGet(MIF_Unsigned_t dataType);

MIF_Pos_t    DB_ElementAdd(DB_String_t *name, short size);
MIF_Status_t DB_ElementDelete(MIF_Pos_t namePos);

/* Enumeration Procedures                                                    */

MIF_Pos_t    DB_enumAdd(MIF_Id_t componentId, DB_String_t *name);
MIF_Status_t DB_enumDelete(MIF_Id_t componentId, MIF_Pos_t enumPos);
MIF_Pos_t    DB_enumElementAdd(MIF_Pos_t enumPos, DB_String_t *symbol,
                            void *value);
MIF_Status_t DB_enumElementDelete(MIF_Pos_t enumPos, MIF_Pos_t elemPos);
MIF_Pos_t    DB_enumFind(MIF_Id_t componentId, DB_String_t *name);

/* Static Table Procedures                                                   */

MIF_Pos_t    DB_tableAdd(MIF_Id_t componentId, MIF_Id_t groupId,
                         DB_String_t *name);
MIF_Status_t DB_tableDelete(MIF_Id_t componentId, MIF_Pos_t tablePos);
MIF_Pos_t    DB_tableElementAdd(MIF_Pos_t tablePos, MIF_Unsigned_t columnNumber,
                                MIF_Unsigned_t rowNumber, MIF_Pos_t dataPos);
MIF_Pos_t    DB_tableFind(MIF_Id_t componentId, MIF_Id_t tableId);
MIF_Pos_t    DB_tableRowFind(MIF_Pos_t tablePos, MIF_Unsigned_t rowNumber);
MIF_Pos_t    DB_tableElementColumnFind(MIF_Pos_t tablePos,
                                       MIF_Unsigned_t columnNumber,
                                       MIF_Unsigned_t rowNumber);
MIF_Pos_t    DB_tableElementRowFind(MIF_Pos_t tablePos,
                                    MIF_Unsigned_t columnNumber,
                                    MIF_Unsigned_t rowNumber);

/* Group Procedures                                                          */

MIF_Pos_t    DB_groupAdd(MIF_Id_t componentId, MIF_Id_t groupId,
                         DB_String_t *name);
MIF_Status_t DB_groupDelete(MIF_Id_t componentId, MIF_Pos_t groupPos);
MIF_Pos_t    DB_groupFindFirst(MIF_Id_t componentId);
MIF_Pos_t    DB_groupFindNext(MIF_Pos_t groupPos);
MIF_Pos_t 	 DB_groupIdToPos(MIF_Id_t componentId, MIF_Id_t groupId);
MIF_Id_t     DB_groupInsertionPointFind(MIF_Id_t componentId, MIF_Id_t groupId);
MIF_Id_t     DB_groupPosToId(MIF_Pos_t groupPos);

/* Key Procedures                                                            */

MIF_Pos_t    DB_keyAdd(MIF_Id_t componentId, MIF_Id_t groupId, DB_Key_t *key);
MIF_Status_t DB_keyDelete(MIF_Pos_t keyPos);
MIF_Bool_t   DB_keyFind(MIF_Pos_t keyPos, MIF_Id_t keyValue);

/* String Macros                                                             */

#define DB_iString(name, string) DB_String_t name = { sizeof(string) - 1, \
                                                      string }
#define DB_uString(name, size) char name[(size) + sizeof(unsigned long)]

/* String Procedures                                                         */

MIF_Pos_t     DB_stringAdd(DB_String_t *string);
MIF_Status_t  DB_stringDelete(MIF_Pos_t strPos);
short         DB_stringCmp(DB_String_t *string1, DB_String_t *string2);
DB_String_t   *DB_stringCpy(DB_String_t *destination, DB_String_t *source);
DB_String_t   *DB_stringInit(DB_String_t *destination, char *source);
DB_String_t   *DB_stringSet(DB_String_t *destination, char c);
unsigned long DB_stringSize(DB_String_t *string);

/************************ PRIVATE ************************************/
/*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
