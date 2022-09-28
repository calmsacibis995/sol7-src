/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_api.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: db_api.h
    

    Description: MIF Database API Routines Header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_api.h 1.31 1994/04/08 09:07:10 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        05/06/93 sfh    Add MIF_keyFind.
        5/13/93  aip    Added MIF_ACCESS_METHOD definition.
        05/18/93 sfh    Revise to use MIF_String_t instead of char[]
                        for strings.
        5/25/93  aip    Changed revision to 1.4 for enumeration support.
        05/28/93 sfh    Remove enumeration procedures.
        6/7/93   aip    Changed revision to 1.5 for Static Table support.
        6/22/93  aip    Changed revision to 1.6 for object copy support.
        6/25/93  aip    Changed revision to 1.7 for new data object.
        07/15/93 sfh    Changed revision to 1.71.
        07/20/93 sfh    Added MIF_classStrGet.
        07/21/93 sfh    Change attributeDataGet to take parameter indicating
                        MIF_READ or MIF_WRITE.
                        Add attributeDescGet, groupDescGet, componentDescGet.
        07/26/93 sfh    Include dos_mi.h
        7/27/93  aip    Changed _MSDOS to MSDOS.
        7/28/93  aip    Using new dmi.h
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
        10/15/93 sfh    Revise dataValueGet to take access method so can
                        read or write data value.
        10/28/93 sfh    Change dos_dmi.h to os_dmi.h.
        12/6/93  aip    Added prototype for MIF_componentPathTypeGet.
                        Changed revision to 1.81
                        Changed path block.
        12/7/93  aip    Added prototypes for component locking.
                        Changed revision to 1.90
        12/16/93 sfh    Revise for asm version compatibilty.
        12/21/93 sfh    Remove _PASCAL.
        02/17/94 sfh    Convert ASM_BUILD to PSL_BUILD ifdefs.
        3/22/94  aip    Removed all _FAR modifiers.
        4/6/94   aip    Added 32-bit string support.
		04/07/94 sfh	Add MIF_componentIdLastInstalledIdGet.
                 aip    New path structure.
        4/8/94   aip    Rev change to 1.91
        05/18/94 par    Changed MIF_String_t to DMI_STRING
        09/18/94 par    Added MIF_writeCache() to library, to allow the cache
                        to be used as write through.  This will help solve
                        the database corruption problems that currently
                        occur in the case of a messy termination.
        02/12/95 par    Reved the version to 2.0 to reflect the removal of the
                        Hash information, and the new cache.

**********************************************************************/

#ifndef DB_API_H_FILE
#define DB_API_H_FILE

/************************ DEFINES ************************************/

#define DB_MAGIC_NUMBER         16985
#define DB_VERSION              "MIF Database Engine v3.00"    /* DMI Version 1.1 database */
#define DB_VERSION_OLD          "MIF Database Engine v2.00"    /* DMI Version 1.0 database */
#define DB_DEFAULT_INDEX_COUNT  256
#define DB_DEFAULT_BUCKET_COUNT 251

#ifdef SL_BUILD

#define MIF_ACCESS_METHOD MIF_READ

#else

#define MIF_ACCESS_METHOD MIF_WRITE

#endif

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ INCLUDES ***********************************/

#include "db_int.h"
#include "os_dmi.h"
#include "mif_db.h"

/*********************************************************************/

/************************ PUBLIC *************************************/

#ifdef __cplusplus
extern "C" {
#endif

DB_Data_t 		*MIF_attributeDataGet(DB_Attribute_t *attributePtr,
									   MIF_IoMode_t accessMethod);
DMI_STRING 	*MIF_attributeDescGet(MIF_Id_t componentId,
									MIF_Id_t groupId, MIF_Id_t attributeId);
DB_Attribute_t 	*MIF_attributeFindNext(MIF_Id_t componentId,
									MIF_Id_t groupId, MIF_Id_t attributeId);
DB_Attribute_t 	*MIF_attributeGet(MIF_Id_t componentId,
									MIF_Id_t groupId, MIF_Id_t attributeId);
DMI_STRING 	*MIF_attributeNameGet(DB_Attribute_t *attributePtr);

DMI_STRING 	*MIF_classStrGet(MIF_Id_t componentID,
														MIF_Id_t groupID);

DMI_STRING 	*MIF_componentDescGet(MIF_Id_t componentId);
DB_Component_t 	*MIF_componentFindNext(MIF_Id_t componentId);
DB_Component_t 	*MIF_componentGet(MIF_Id_t componentId);
DMI_UNSIGNED	MIF_componentLastInstalledIdGet(void);
MIF_Status_t 	MIF_componentLockClear(MIF_Id_t componentId);
MIF_MaHandle_t  MIF_componentLockGet(MIF_Id_t componentId);
short 			MIF_componentLockTestAndSet(MIF_Id_t componentId,
                                             MIF_MaHandle_t maHandle);
DMI_STRING 	*MIF_componentNameGet(DB_Component_t *componentPtr);
DMI_STRING 	*MIF_componentPathGet(MIF_Id_t componentId,
                                       MIF_Environment_t environmentId,
                                       DMI_STRING *symbolicName);
MIF_PathType_t	MIF_componentPathTypeGet(MIF_Id_t componentId,
                                         MIF_Environment_t environmentId,
                                         DMI_STRING *symbolicName);

void 			*MIF_dataValueGet(DB_Data_t *dataPtr,
                                  MIF_IoMode_t accessMethod);

DMI_STRING 	*MIF_groupDescGet(MIF_Id_t componentId, MIF_Id_t groupId);
DB_Group_t 		*MIF_groupFindNext(MIF_Id_t componentId, MIF_Id_t groupId);
DB_Group_t 		*MIF_groupGet(MIF_Id_t componentId, MIF_Id_t groupId);
DMI_STRING 	*MIF_groupNameGet(DB_Group_t *groupPtr);
DB_Key_t 		*MIF_keyFind(MIF_Id_t componentId, MIF_Id_t groupId);

MIF_Status_t     MIF_databaseClose(void);
MIF_Status_t     MIF_databaseCreate(char *filename, char *version,
                                    short indexCount, short bucketCount);
MIF_Status_t     MIF_databaseOpenRead(char *filename, char *version);
MIF_Status_t 	 MIF_databaseOpenReadWrite(char *filename, char *version);

char             *MIF_dateTime(void);
char             *MIF_version(void);
MIF_Status_t      MIF_writeCache(void);  /* force a write of the currently buffered cache contents */

/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif
