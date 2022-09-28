/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_api.c	1.2 96/09/24 Sun Microsystems"

/**********************************************************************

	Filename: db_api.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Database API Routines

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
		05/06/93 sfh	Add MIF_keyFind.
		05/07/93 sfh	Fix MIF_keyFind to return NULL if key slot or
						group position are empty; call MIF_resolve with
						MIF_READ, not MIF_WRITE.
        5/13/93  aip    Changed "get" routines to be in MIF_WRITE mode for
                        the installer.
                        MIF_componentPathGet will search through all
                        environments if a NIL pointer is passed in.
		05/18/93 sfh	Revise to use MIF_String_t instead of char[]
						for strings;
		5/26/93	 aip    Added enumeration procedures.
		05/28/93 sfh	Remove enumeration procedures.
		07/20/93 sfh	Added MIF_classStrGet.
		07/21/93 sfh	Change attributeDataGet to take parameter indicating
						MIF_READ or MIF_WRITE.
				   		Exclude databaseOpenRead.  Now opening read/write.
						Add attributeDescGet, groupDescGet, componentDescGet.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
		10/28/93 sfh	Remove MSDOS ifdefs.  We now make a local copy of
						all far strings.
		11/04/93 sfh	Add call to DB_headerFlush in MIF_databaseClose.
        12/6/93  aip    Added MIF_componentPathTypeGet procedure.
                        Changed path block.
        12/7/93  aip    Added component locking.
		12/09/93 sfh	Revise componentLockTestAndSet to allow resetting
						by same mgmt app.
		12/14/93 sfh	Change groupFindNext and attributeFindNext to not
						call insertion point function; it's not needed.
        3/22/94  aip    Removed all _FAR modifiers.
        4/6/94   aip    New path structure.
		04/07/94 sfh	Add MIF_componentLastInstalledIdGet.
		04/08/94 sfh	Remove ifdef SL_BUILD from call to DB_headerFlush in 
						MIF_databaseClose.
        05/18/94 par    Changed MIF_String_t to DMI_STRING type
        09/18/94 par    Added MIF_writeCache() to library, to allow the cache
                        to be used as write through.  This will help solve
                        the database corruption problems that currently
                        occur in the case of a messy termination.

************************* INCLUDES ***********************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "os_dmi.h"
#include "db_api.h"
#include "db_hdr.h"
#include "db_index.h"
#include "db_lock.h"
#include "mif_db.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

MIF_FileHandle_t DB;

/*********************************************************************/

MIF_Status_t MIF_writeCache(void)  /* force a write of the currently buffered cache contents */
{
return MIF_cacheFlush(DB,MIF_CACHE_WRITETHRU);
}

DB_Key_t *MIF_keyFind(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Pos_t	groupPos;
    DB_Group_t	*g;

	groupPos = DB_groupIdToPos(componentId, groupId);

	if (groupPos.page == 0)
		return NULL;

	if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) == NULL)
	    return NULL;

	if (g->key.page == 0)
		return NULL;

	return (DB_Key_t *)MIF_resolve(DB, g->key, MIF_READ);
}


DB_Data_t *MIF_attributeDataGet(DB_Attribute_t *attributePtr,
													MIF_IoMode_t accessMethod)
{
    if (attributePtr == (DB_Attribute_t *) 0)
        return (DB_Data_t *) 0;

    return (DB_Data_t *)
           MIF_resolve(DB, attributePtr -> dataPos, accessMethod);
}


DMI_STRING *MIF_attributeDescGet(MIF_Id_t componentId, MIF_Id_t groupId,
									MIF_Id_t attributeId)
{
	DB_Attribute_t			*attrib;

	attrib = MIF_attributeGet(componentId, groupId,	attributeId);

	if (attrib == NULL)
		return NULL;

	if (attrib->description.page == 0)
		return NULL;

	return (DMI_STRING *)MIF_resolve(DB, attrib->description, MIF_READ);
}


DB_Attribute_t *MIF_attributeFindNext(MIF_Id_t componentId, MIF_Id_t groupId,
                                      MIF_Id_t attributeId)
{
    MIF_Pos_t attributePos;

    if (attributeId == 0)
        attributePos = DB_attributeFindFirst(componentId, groupId);
    else
        attributePos = DB_attributeFindNext(DB_attributeIdToPos(componentId,
                                                        groupId, attributeId));

    if ((attributePos.page == 0) && (attributePos.offset == 0))
        return (DB_Attribute_t *) 0;

    return (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ);
}

DB_Attribute_t *MIF_attributeGet(MIF_Id_t componentId, MIF_Id_t groupId,
                                 MIF_Id_t attributeId)
{
    MIF_Pos_t      attributePos;
    DB_Attribute_t *a;

    attributePos = DB_attributeFindFirst(componentId, groupId);
    while ((attributePos.page != 0) || (attributePos.offset != 0)) {
        if ((a = (DB_Attribute_t *)
            MIF_resolve(DB, attributePos, MIF_ACCESS_METHOD)) ==
            (DB_Attribute_t *) 0)
            return (DB_Attribute_t *) 0;
        if (a -> id == attributeId)
            return a;
        attributePos = DB_attributeFindNext(attributePos);
    }
    return (DB_Attribute_t *) 0;
}

DMI_STRING *MIF_attributeNameGet(DB_Attribute_t *attributePtr)
{
    MIF_Pos_t namePos;

    if (attributePtr == (DB_Attribute_t *) 0)
        return (DMI_STRING *) 0;

    namePos.page = attributePtr -> name.name.page;
    namePos.offset = attributePtr -> name.name.offset;
    if ((namePos.page == 0) && (namePos.offset == 0))
        return (DMI_STRING *) 0;

    return (DMI_STRING *) MIF_resolve(DB, namePos, MIF_READ);
}


DMI_STRING *MIF_classStrGet(MIF_Id_t componentID, MIF_Id_t groupID)
{
	DB_Group_t	*group;

	if ((group = MIF_groupGet(componentID, groupID)) == NULL)
		return NULL;

	if (group->classStr.page == 0)
		return NULL;

	return (DMI_STRING *)MIF_resolve(DB, group->classStr, MIF_READ);
}


DMI_STRING *MIF_componentDescGet(MIF_Id_t componentId)
{
	DB_Component_t			*comp;

	comp = MIF_componentGet(componentId);

	if (comp == NULL)
		return NULL;

	if (comp->description.page == 0)
		return NULL;

	return (DMI_STRING *)MIF_resolve(DB, comp->description, MIF_READ);
}


DB_Component_t *MIF_componentFindNext(MIF_Id_t componentId)
{
    MIF_Id_t      index;
    DB_Component_t *componentPtr;

    if (componentId > DB_DEFAULT_INDEX_COUNT)
        return (DB_Component_t *) 0;

    for (index = componentId + 1;
        index <= (MIF_Id_t) DB_headerIndexTableLastIndexGet(DB);
        ++index) {
        componentPtr = MIF_componentGet(index);
        if (componentPtr != (DB_Component_t *) 0)
            return componentPtr;
    }
    return (DB_Component_t *) 0;
}

DB_Component_t *MIF_componentGet(MIF_Id_t componentId)
{
    MIF_Pos_t componentPos;

    componentPos = DB_componentIdToPos(componentId);
    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return (DB_Component_t *) 0;

    return (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_ACCESS_METHOD);
}

DMI_UNSIGNED MIF_componentLastInstalledIdGet(void)
{
	return DB_headerIndexTableLastIndexGet(DB);
}

MIF_Status_t MIF_componentLockClear(MIF_Id_t componentId)
{
    return DB_lockSet((unsigned short) componentId, 0);
}

MIF_MaHandle_t MIF_componentLockGet(MIF_Id_t componentId)
{
MIF_MaHandle_t Handle;

    if(DB_lockGet((unsigned short) componentId,&Handle) == MIF_OKAY) return Handle;
    else return (MIF_MaHandle_t)NULL;
}

/*
    The return from MIF_componentLockTestAndSet is as follows:

    If the component is not previously locked and the lock succeeds: MIF_TRUE
    If the component is locked by the same mgmt app: MIF_TRUE
    If the component is locked by another mgmt app: MIF_FALSE
    
    Otherwise, some sort of database file error occured: -1
*/

short MIF_componentLockTestAndSet(MIF_Id_t componentId, MIF_MaHandle_t maHandle)
{
MIF_MaHandle_t handle;

    if(DB_lockGet((unsigned short) componentId,&handle) == MIF_BAD_ID) return -1;

/* If component locked by other than requesting mgmt app, fail               */
    if (handle != 0 && handle != maHandle)
        return MIF_FALSE;

    if (DB_lockSet((unsigned short) componentId, maHandle) != MIF_OKAY)
        return -1;

    return MIF_TRUE;
}

DMI_STRING *MIF_componentNameGet(DB_Component_t *componentPtr)
{
    MIF_Pos_t namePos;

    if (componentPtr == (DB_Component_t *) 0)
        return (DMI_STRING *) 0;

    namePos.page = componentPtr -> name.name.page;
    namePos.offset = componentPtr -> name.name.offset;
    if ((namePos.page == 0) && (namePos.offset == 0))
        return (DMI_STRING *) 0;

    return (DMI_STRING *) MIF_resolve(DB, namePos, MIF_READ);
}

DMI_STRING *MIF_componentPathGet(MIF_Id_t componentId,
                                   MIF_Environment_t environmentId,
                                   DMI_STRING *symbolicName)
{
    MIF_Pos_t          cpathPos;
    DB_ComponentPath_t *cp;
    DMI_STRING       *c;

    cpathPos = DB_componentPathFindFirst(componentId);
    while ((cpathPos.page != 0) || (cpathPos.offset != 0)) {
        if ((cp = (DB_ComponentPath_t *) MIF_resolve(DB, cpathPos, MIF_READ)) ==
            (DB_ComponentPath_t *) 0)
            return (DMI_STRING *) 0;
        if ((environmentId == MIF_UNKNOWN_ENVIRONMENT) ||
            (environmentId == cp -> iEnvironmentId)) {
            if ((c = (DMI_STRING *)
                     MIF_resolve(DB, cp -> name.name, MIF_READ)) ==
                (DMI_STRING *) 0)
                return (DMI_STRING *) 0;
            if ((c -> length == symbolicName -> length) &&
                strncmp(c -> body, symbolicName -> body,
                        (size_t) c -> length) == 0) {
                return (DMI_STRING *) MIF_resolve(DB, cp -> pathValue,
                                                    MIF_READ);
            }
        }
        cpathPos = DB_componentPathFindNext(cpathPos);
    }
    return (DMI_STRING *) 0;
}

MIF_PathType_t MIF_componentPathTypeGet(MIF_Id_t componentId,
                                        MIF_Environment_t environmentId,
                                        DMI_STRING *symbolicName)
{
    MIF_Pos_t          cpathPos;
    DB_ComponentPath_t *cp;
    DMI_STRING       *c;

    cpathPos = DB_componentPathFindFirst(componentId);
    while ((cpathPos.page != 0) || (cpathPos.offset != 0)) {
        if ((cp = (DB_ComponentPath_t *) MIF_resolve(DB, cpathPos, MIF_READ)) ==
            (DB_ComponentPath_t *) 0)
            return MIF_UNKNOWN_PATH_TYPE;
        if (environmentId == cp -> iEnvironmentId) {
            if ((c = (DMI_STRING *)
                     MIF_resolve(DB, cp -> name.name, MIF_READ)) ==
                (DMI_STRING *) 0)
                return MIF_UNKNOWN_PATH_TYPE;
            if ((c -> length == symbolicName -> length) &&
                strncmp(c -> body, symbolicName -> body,
                        (size_t) c -> length) == 0) {
                if ((cp = (DB_ComponentPath_t *)
                    MIF_resolve(DB, cpathPos, MIF_READ)) ==
                    (DB_ComponentPath_t *) 0)
                    return MIF_UNKNOWN_PATH_TYPE;
                else
                    return cp -> pathType;
            }
        }
        cpathPos = DB_componentPathFindNext(cpathPos);
    }
    return MIF_UNKNOWN_PATH_TYPE;
}

DMI_STRING *MIF_groupDescGet(MIF_Id_t componentId, MIF_Id_t groupId)
{
	DB_Group_t			*group;

	group = MIF_groupGet(componentId, groupId);

	if (group == NULL)
		return NULL;

	if (group->description.page == 0)
		return NULL;

	return (DMI_STRING *)MIF_resolve(DB, group->description, MIF_READ);
}


DB_Group_t *MIF_groupFindNext(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Pos_t groupPos;

	if (groupId == 0)
        groupPos = DB_groupFindFirst(componentId);
    else
        groupPos = DB_groupFindNext(DB_groupIdToPos(componentId, groupId));

	if (groupPos.page == 0)
		return NULL;

    return (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ);
}

DB_Group_t *MIF_groupGet(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Pos_t groupPos;

    groupPos = DB_groupIdToPos(componentId, groupId);
    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return (DB_Group_t *) 0;
    return (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_ACCESS_METHOD);
}

DMI_STRING *MIF_groupNameGet(DB_Group_t *groupPtr)
{
    MIF_Pos_t namePos;

    if (groupPtr == (DB_Group_t *) 0)
        return (DMI_STRING *) 0;

    namePos.page = groupPtr -> name.name.page;
    namePos.offset = groupPtr -> name.name.offset;
    if ((namePos.page == 0) && (namePos.offset == 0))
        return (DMI_STRING *) 0;

    return (DMI_STRING *) MIF_resolve(DB, namePos, MIF_READ);
}

void *MIF_dataValueGet(DB_Data_t *dataPtr, MIF_IoMode_t accessMethod)
{
    if (dataPtr == (DB_Data_t *) 0)
        return (void *) 0;

    return MIF_resolve(DB, dataPtr -> valuePos, accessMethod);
}

MIF_Status_t MIF_databaseClose(void)
{
    MIF_FileHandle_t databaseHandle;

    if (DB == 0)
        return MIF_OKAY;

    databaseHandle = DB;
    DB = 0;

	DB_headerFlush(databaseHandle);

    return MIF_close(databaseHandle);
}

#ifndef SL_BUILD

MIF_Status_t MIF_databaseCreate(char *filename, char *version,
                                short indexCount, short bucketCount)
{
    char filenameCopy[FILENAME_MAX];
    char versionCopy[80];


    strcpy(filenameCopy, filename);	
    strcpy(versionCopy, version);
    if ((DB = MIF_create(filenameCopy, DB_MAGIC_NUMBER, versionCopy)) == 0)
        return MIF_FILE_ERROR;
    if (DB_indexTableCreate(indexCount) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_lockTableCreate(indexCount) != MIF_OKAY)
        return MIF_FILE_ERROR;
    return MIF_OKAY;
}

#endif

#ifndef SL_BUILD

MIF_Status_t MIF_databaseOpenRead(char *filename, char *version)
{
    char filenameCopy[FILENAME_MAX];

    strcpy(filenameCopy, filename);
    if((DB = MIF_open(filenameCopy, MIF_READ, DB_MAGIC_NUMBER)) == 0)
        return MIF_NOT_FOUND;
    else if (strcmp(MIF_headerVersionGet(DB), version) != 0)
        return MIF_BAD_FORMAT;
    else
        return MIF_OKAY;
}

#endif

MIF_Status_t MIF_databaseOpenReadWrite(char *filename, char *version)
{
    if ((DB = MIF_open(filename, MIF_WRITE, DB_MAGIC_NUMBER)) == 0)
        return MIF_NOT_FOUND;

    if (strcmp(MIF_headerVersionGet(DB), version) != 0)
        return MIF_BAD_FORMAT;

    return DB_lockTableClear();
}

#ifndef SL_BUILD

char *MIF_dateTime(void)
{
    time_t ltime;

    ltime = MIF_headerTimeGet(DB);
    if (ltime == 0)
        return (char *) 0;
    else
        return ctime(&ltime);
}

#endif

#ifndef SL_BUILD

char *MIF_version(void)
{
    return MIF_headerVersionGet(DB);
}

#endif
