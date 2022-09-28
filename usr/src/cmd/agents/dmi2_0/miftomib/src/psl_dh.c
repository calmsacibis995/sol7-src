/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_dh.c	1.2 96/09/24 Sun Microsystems"


/*****************************************************************************************************************************************************
    Filename: psl_dh.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994 - 1996

    Description: Database Handler

    Author(s): Alvin I. PIvowar, Paul A. Ruocchio

    RCS Revision: $Header: n:/sl/rcs/psl_dh.c 1.92 1994/07/05 15:28:54 shanraha Exp $

    Revision History:

		Date		Author Description
		-------	 	---		-----------------------------------------------
		3/24/93	 	aip		Creation date.
		10/20/93 	sfh		Add above change to second place in listComponents that calls db looking for a component.
		11/02/93 	sfh		Fix DH_getKeys to not return error if a DmiGetFirstRowCmd does not specify keys.
		11/04/93 	sfh		Change errorCode in getKeys to DBERR_ROW_NOT_FOUND instead of DBERR_GROUP_NOT_FOUND.
		11/11/93 	sfh		Remove include of types.h.
		11/17/93 	sfh		Add row number as parameter to get row commands.  Allows us to avoid searching database for each key and attribute of 
							a row of a table.
 		12/07/93 	sfh		Decrement response count in getKeys after building group response.  Set to one in getRow when keys all built.
		12/08/93 	sfh		Add groupId to get attribute responses.
							Verify key type in checkKeys.  Remove check for	invalid key type in getKeys, since type is already checked.
		12/09/93 	sfh		Check component path to ensure that attributes specified as direct are handled by direct code.
		12/09/93 	sfh		Add locking to getOrSetAttribute and getRow.
		12/16/93 	sfh		Revise checkKeys to call MIF_keyFind, since all necessary info is there; revise procedures that call checkKeys.  
							Changes in checkKeys caused previous address returned by keyFind to be invalid due to caching.
		12/16/93 	sfh		Fix getAttributeData: If tableElementRowFind (after table search) returns NULL, there are 2 possibilites: 1) The row 
							requested does not exist; or 2) the column requested is a default value (in this case, it is not part of the table).  For (2) we want to 
							use default	value.
		01/05/94 	sfh		Change error returned from main if bad command to SLERR_ILLEGAL_COMMAND.
		03/04/94	sfh		Add parent/child code.
							Revise getRow to call instrumentation only if attribute access mode is not write only and to set oAttributeValue to 0 if 
							attribute is write only.
		03/10/94	sfh		Update to 4.3 spec:
								Revise getAttributeData to not search default row after all other rows searched.
								Revise getSetAttribute to return error if attribute's access mode is unsupported.
								Revise getRow to not return unsupported attributes.
							Revise parent/child code.
							Convert fstring functions to string.
		04/06/94	sfh		Break listComponents into two parts so it can be called from MH without doing a task switch.
		04/07/94	sfh		MIF_String -> DMI_STRING.
		04/08/94	sfh		Revise for new component path IDs.
							Revise DH_init to build path name before opening db file.
		05/10/94	sfh		Update to 1.0 (draft 4.6) spec -
								- Change classCmp to do case sensitive compare.
								- Change osClassName to osClassString.
		05/11/94	sfh		Fix getAttributeData to increment row properly.
		05/18/94	sfh		Revise buildInstrumentation to set values into GetAttribCnf struct.  Revise calls to this to pass data type.
        05/19/94    par         Modified for use with the new 2.x level PSL
		05/23/94	sfh		Fix getKeys to decrement iGroupKeyCount after calling buildGroupKey when instrumentation will be called to supply value.
        06/04/94    par         Fixed GetorSetAttributes() to handle MIF_DATE types correctly
        06/18/94    par         Changed name to conform to new naming rules
		07/05/94	sfh		Modify getAttributeData to, if attribute is not found in db, see if it is in an overlay and return the DB_Data_t
							corresponding to it if so.
        09/09/94    par     Corrected lengths in string types when returned in the GetRow commands,
                            they were getting truncated by the length of the length field.
        09/09/94    par     Corrected the MAX size for an attribute that is returned for a string type
                            on the list attribute command.
        01/24/96    par     Modified to support Version 1.1
																																															
*****************************************************************************************************************************************************/


/********************************************************************* INCLUDES *********************************************************************/

#include <stdlib.h>
#include <string.h>
#include "psl_main.h"
#include "mif_db.h"
#include "db_api.h"
#include "psl_dh.h"
#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */
#include "psl_util.h"
#include "process.h"

/*************************************************************************************************************************************************/


/******************************************************************* DEFINES *********************************************************************/

/*************************************************************************************************************************************************/


/******************************************************************* TYPEDEFS ********************************************************************/
/* Controls whether getTableValue returns value from current row */
/* or next row (for group get keys) */
typedef enum {DH_RETURN_THIS_ROW, DH_RETURN_NEXT_ROW} DH_TableReturn_t;

/****************************************************************************************************************************************************/


/********************************************************************* DATA **********************************************************************/

extern MIF_FileHandle_t DB;
char DH_mifDbName[] = "sldb.dmi";
char    *SL_path;                       /* Path sl is in.  Allocated and initialized at startup. */

/****************************************************************************************************************************************************/


/************************************************************** FUNCTION PROTOTYPES *****************************************************************/

static void DH_getOrSetAttributes(DMI_GetAttributeReq_t _FAR *gCommand);
static void DH_getRow(DMI_GetRowReq_t _FAR *gCommand, unsigned int row);
static void DH_listAttributes(DMI_ListAttributeReq_t _FAR *lCommand);
static void DH_listComponents(DMI_ListComponentReq_t _FAR *lCommand);
static void DH_listGroups(DMI_ListGroupReq_t _FAR *lCommand);
static void DH_listDescription(DMI_ListDescReq_t _FAR *dmiCommand);
static void DH_ListGroupPragma(DMI_ListGroupPragmaReq_t _FAR *dmiCommand);

/****************************************************************************************************************************************************/


void DH_fini(void)
{
    MIF_databaseClose();
}


boolean _FAR DH_init(char _FAR *SL_PathName)
{
	char	*dbPath;
	short		retVal,hold;

    SL_path = SL_PathName;    /* set the system path name */
	dbPath = SL_buildPathname(DH_mifDbName);							/* Mallocs space for name */
	if (dbPath == NULL)
		return FALSE;


	if ((hold = MIF_databaseOpenReadWrite(dbPath, DB_VERSION)) == MIF_OKAY)
		retVal = TRUE;
    else if(hold == MIF_BAD_FORMAT) retVal = FALSE;


    /***** NOTE: this is where we will want to do a database update *****/


    else if(MIF_databaseCreate(dbPath, DB_VERSION,DB_DEFAULT_INDEX_COUNT,DB_DEFAULT_BUCKET_COUNT) == MIF_OKAY)
            retVal = TRUE;
    else
		retVal = FALSE;

	OS_Free(dbPath);


	return (boolean)retVal;
}


/*		** NOTE **  */
/* The procedures in dh.c rely on initializations performed by code in mh.c  */


/***************************************************************************************************************************************************
    Function:	DH_main

   Parameters:	dmiCommand			The command to be processed.
				parameter			Additional parameter.  For GetRow commands, this is the row being retrieved (only after the first attribute has 
															been retrieved; it is unused for other commands

  Description:	This function determines what action to take with dmiCommand.  It calls the appropriate database handler procedure.

      Returns:	This function always returns TM_NOT_DONE.

***************************************************************************************************************************************************/

TM_TaskStatus_t DH_main(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter)
{
    switch (dmiCommand->iCommand) 
    {
		case DmiGetAttributeCmd:
		case DmiSetAttributeCmd:
		case DmiSetReserveAttributeCmd:
		case DmiSetReleaseAttributeCmd:
			DH_getOrSetAttributes((DMI_GetAttributeReq_t _FAR *)dmiCommand);
		    break;
		case DmiGetFirstRowCmd:
		case DmiGetRowCmd:
		case DmiGetNextRowCmd:
    		DH_getRow((DMI_GetRowReq_t _FAR *)dmiCommand,(unsigned int)(unsigned long)parameter);
			return TM_NOT_DONE;
		case DmiListAttributeCmd:
		case DmiListNextAttributeCmd:
			DH_listAttributes((DMI_ListAttributeReq_t _FAR *)dmiCommand);
		    break;
		case DmiListComponentCmd:
		case DmiListNextComponentCmd:
			DH_listComponents((DMI_ListComponentReq_t _FAR *)dmiCommand);
    	    break;
		case DmiListGroupCmd:
		case DmiListNextGroupCmd:
			DH_listGroups((DMI_ListGroupReq_t _FAR *)dmiCommand);
		    break;
        case DmiListGroupPragmaCmd:
            DH_ListGroupPragma((DMI_ListGroupPragmaReq_t _FAR *)dmiCommand);
            break;
		case DmiListAttributeDescCmd:
		case DmiListComponentDescCmd:
		case DmiListGroupDescCmd:
			DH_listDescription((DMI_ListDescReq_t _FAR *)dmiCommand);
			break;
		default:	
		    SL_buildError(dmiCommand, SLERR_ILLEGAL_COMMAND);
			TM_taskSwitch(TM_CALLBACK,(DMI_MgmtCommand_t _FAR *)dmiCommand, NULL);
    }
	return TM_NOT_DONE;
}


static void DH_buildInstrumentationVector(DMI_MgmtCommand_t _FAR *dmiCommand, MIF_Id_t componentId, MIF_Id_t groupId, MIF_Id_t attributeId, DMI_UNSIGNED dataType, DMI_STRING *symbolName)
{
TM_Task_t 								nextTask = TM_CALLBACK;
DMI_GetAttributeCnf_t _FAR				*getAttribCnf;
void _FAR								*cnfBuf;
DMI_InstrumentationVector_t _FAR 		*instrumentationVector = (DMI_InstrumentationVector_t _FAR *)NULL;
DMI_InstrumentationDescriptor_t _FAR	*instrumentationDesc;
DMI_STRING _FAR							*nextPath;
DMI_STRING _FAR							*instrumentationName = (DMI_STRING _FAR *)NULL;
DMI_INT									avail;
SL_ErrorCode_t							errorCode = SLERR_BUFFER_FULL;
DMI_STRING 								*symbolicName = NULL;
DMI_UNSIGNED							command;
DMI_UNSIGNED							ourIndex = MIF_NUM_ENVIRONMENTS;		/* For our SL, if found; initially invalid index */
DMI_STRING 		*pathName;
DMI_OFFSET 		offset;

    getAttribCnf = SL_buildCiCommand(dmiCommand);						/* Prepare for instrumentation */
    if (getAttribCnf == NULL)  goto done;														/* Error already built */

    /* Insert return values (except oAttribValue, of course) */
    getAttribCnf->iGroupId = groupId;
    getAttribCnf->iAttributeId = attributeId;
    getAttribCnf->iAttributeType = dataType;
    cnfBuf = dmiCommand->pCnfBuf;
    command = dmiCommand->iCommand;

/* The instrumentation vector resides after the last GetAttributeCnf struct (or after the GetRowCnf struct if no attrbibutes built yet). */
/* For the set commands, the parent has built a confirm buffer that we filled in; the instrumentation vector and the confirm buffer are the same. */
/* This is the same as pCnfBuf in the DmiCiCommand. */

    instrumentationVector = (DMI_InstrumentationVector_t _FAR *)((char _FAR *)(dmiCommand->DmiCiCommand.pCnfBuf) + sizeof(DMI_GetAttributeCnf_t) );
    instrumentationDesc = instrumentationVector->DmiInstrumentation;

/* May be no instrumentation name if direct interface preempts MIF or overlay */
    if (symbolName != NULL){ /* Due to database cache buffers, we need to save symbolName  */
        symbolicName = OS_Alloc(stringLen(symbolName));
        if (symbolicName == NULL){
            errorCode = SLERR_OUT_OF_MEMORY;
            goto error;
        }
        stringCpy(symbolicName, symbolName);
        instrumentationName = (DMI_STRING _FAR *) ((char _FAR *)instrumentationDesc + sizeof(DMI_InstrumentationDescriptor_t) );
        stringCpy(instrumentationName,symbolicName);
        instrumentationVector->osInstrumentationName = (char _FAR *)instrumentationName - (char _FAR *)cnfBuf;
    }
    else   instrumentationVector->osInstrumentationName = 0;

    avail = dmiCommand->DmiCiCommand.iCnfBufLen;							/* What we have to play with */
    avail -= MIF_NUM_ENVIRONMENTS * sizeof(DMI_InstrumentationDescriptor_t);	
    if (avail < 0)  goto error;

    nextPath = (DMI_STRING _FAR *) ((char _FAR *)instrumentationName + stringLen(symbolicName) );
    instrumentationVector->countSelector.iCount = 0;

    /* See if attribute overridden by direct instrumentation first */
    if ((MH_directLookup(componentId, groupId, attributeId,SL_ENVIRONMENT_ID)) == NULL){
        if(MIF_componentPathTypeGet(componentId, SL_ENVIRONMENT_ID, symbolicName) == MIF_DIRECT_INTERFACE_PATH_TYPE){
            errorCode = DBERR_DIRECT_INTERFACE_NOT_REGISTERED;
            goto error;
        }
        if(symbolicName == NULL){    /* nobody home, get out of here */
            errorCode = DBERR_ATTRIBUTE_NOT_FOUND;
            goto error;
        }
        pathName = MIF_componentPathGet(componentId, SL_ENVIRONMENT_ID, symbolicName);
        if (pathName == NULL){  /*  No instrumentation for this environment */
            errorCode = DBERR_ATTRIBUTE_NOT_FOUND;
            goto error;
        }
        avail -= stringLen(pathName);
        if (avail < 0) goto error;
        stringCpy(nextPath, pathName);
        offset = (char _FAR *)nextPath - (char _FAR *)cnfBuf;
        nextPath = (DMI_STRING _FAR *) ((char _FAR *)nextPath + stringLen(pathName));
    }
    else offset = 0;     /* High perf is registered */

	instrumentationDesc[instrumentationVector->countSelector.iCount].iEnvironmentId = SL_ENVIRONMENT_ID;
	instrumentationDesc[instrumentationVector->countSelector.iCount].osPathname = offset;
	instrumentationVector->countSelector.iSelector = instrumentationVector->countSelector.iCount; 

	nextTask = TM_EXECUTE_INSTRUMENTATION;
	goto done;

error:
	SL_buildError(dmiCommand, errorCode);


done:
	if (symbolicName != NULL)
		OS_Free(symbolicName);

	TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)dmiCommand, instrumentationVector);
}


static boolean DH_checkKeys(DMI_UNSIGNED componentId, DMI_UNSIGNED  groupId,
				DMI_UNSIGNED keyCount, DMI_GroupKeyData_t _FAR *groupKeys,char _FAR *MAXBUF,SL_ErrorCode_t *errorCode)
{
	DB_Key_t		*keys;
	DMI_UNSIGNED	i;

	if (keyCount == 0)
		return TRUE;

	keys = MIF_keyFind(componentId, groupId);

/* Make sure group has a key structure */
	if (keys == NULL)
		return FALSE;

	if (keyCount != keys->keyCount)
		return FALSE;

/* Keys must be in order! */
	for (i = 0; i < keyCount; i++){
        if((char _FAR *)&(groupKeys[i]) >= MAXBUF){  /* bad buffer error */
            (*errorCode) = SLERR_ILL_FORMED_COMMAND;
            return FALSE;
        }
		if (groupKeys[i].iAttributeId != keys->keys[i])
			return FALSE;
    }

	for (i = 0; i < keyCount; i++)
	{
		DB_Attribute_t	*attr;

		attr = MIF_attributeGet(componentId, groupId, groupKeys[i].iAttributeId);
		if ((attr == (DB_Attribute_t *)NULL) || (groupKeys[i].iAttributeType != attr->dataType))
			return FALSE;
	}
	return TRUE;
}


/* Note that the row of the table is returned so that we don't have to search starting from row one for every key/attribute in the row */
static DB_Data_t *DH_getAttributeData(DMI_MgmtCommand_t _FAR *dmiCommand, DMI_UNSIGNED componentId, DMI_UNSIGNED groupId, DMI_UNSIGNED attribId,
											unsigned short keyCount, DMI_GroupKeyData_t _FAR *groupKeys, DH_TableReturn_t returnType, unsigned short *row)
{
	MIF_Pos_t			tablePos;
	DB_Attribute_t		*attrib;
	unsigned int			rowCount;
	DB_TableElement_t	*te;
	MIF_Pos_t			pos;
	DB_Data_t			*data;
	unsigned int			i;
	unsigned int  			dataSize;
	unsigned int			accessMethod;
	unsigned int			command = (unsigned int)dmiCommand->iCommand;
    BOOL                lookingForKey = FALSE;

	if (command == DmiSetAttributeCmd)
		accessMethod = MIF_WRITE;
	else
		accessMethod = MIF_READ;

	/* Already know group isn't null.  tablePos may be 0 */
	tablePos = MIF_groupGet(componentId, groupId)->tableLink;		

/* NOTE:  This function relies on the DB_table... functions checking the table pos for 0. */

/* If no keys or GET_FIRST_GROUP, use first row or default attribute */
	if (keyCount == 0 || command == DmiGetFirstRowCmd)
	{
		pos = DB_tableElementRowFind(tablePos, attribId, 1);
		if (pos.page != 0)
		{
			te = MIF_resolve(DB, pos, MIF_READ);
			return MIF_resolve(DB, te->dataPos, accessMethod);
		}
		attrib = MIF_attributeGet(componentId, groupId, attribId);
		if (attrib == NULL)
			return NULL;

		return MIF_attributeDataGet(attrib, accessMethod);
	}

/* Convert from NULL parameter passed into to TaskSwitch to row 1 */
	if (*row == 0)
		*row = 1;
	else
		goto row_found;					/* Skip the search -- we already know the row */

/* If not table, we go through the loop once and get "default" attr values */
	if (tablePos.page != 0)
		rowCount = (unsigned int)((DB_Table_t *)(MIF_resolve(DB,
											tablePos, MIF_READ)))->rowCount;
	else
		rowCount = 1;

	for (; *row <= rowCount; (*row)++)
	{
	
/* Compare all key values (note that key IDs have been verified) */
		for (i = 0; i < keyCount; i++)
		{
            if(groupKeys[i].iAttributeId == attribId) lookingForKey = TRUE;
			pos = DB_tableElementColumnFind(tablePos, groupKeys[i].iAttributeId, *row);
			if (pos.page != 0)											/* Not default value */
			{
				te = MIF_resolve(DB, pos, MIF_READ);
				pos = te->dataPos;
				data = MIF_resolve(DB, pos, MIF_READ);
			}
			else
			{
				attrib = MIF_attributeGet(componentId, groupId, attribId);
				data = MIF_attributeDataGet(attrib, accessMethod);
			}

			if (data->dataType == MIF_COMPONENT_PATH_NAME)
				break;													/* Try next row */

           	dataSize = DB_dataTypeSizeGet(data->dataType);
			if (dataSize == 0)											/* String */
				dataSize = (unsigned int)data->currentSize;

			if (memcmp((void _FAR *)((char _FAR *)dmiCommand + groupKeys[i].oKeyValue), MIF_dataValueGet(data,MIF_READ), dataSize) != 0)
				break;													/* Get out and do next row */
		}

		if (i != keyCount)												/* Not found */
			continue;

/* If we're here, all keys match */
		if (returnType == DH_RETURN_NEXT_ROW)
			(*row)++;													/* So we can return correct row */

row_found:
		pos = DB_tableElementRowFind(tablePos, attribId, *row);
		if (pos.page != 0){
			pos = ((DB_TableElement_t *)(MIF_resolve(DB, pos, MIF_READ)))->dataPos;
			return MIF_resolve(DB, pos, accessMethod);
		}
		else if(!lookingForKey)
            attrib = MIF_attributeGet(componentId,groupId,attribId);
        else{                          /* Not looking for key */
          if (pos.page != 0) return NULL;  /* Table but attribute not found */
          else
              if (returnType == DH_RETURN_NEXT_ROW) return NULL;    /* No next row for non-tables */
          attrib = MIF_attributeGet(componentId, groupId, attribId);
        }
        if(attrib != NULL){
            data = MIF_attributeDataGet(attrib,MIF_READ);
            return data;
        }
        else return NULL; /* Something wrong -- we found the row but the attribute didn't exist! */
    }

/* All rows searched and no match -- If attribute value resides in an overlay, then we return the data element for that value.  The caller */
/* can test the data type to determine the action necessary.  Note that all values in a row are either in the row are either in the overlay */
/* or in the database -- there is no mixing. */

	attrib = MIF_attributeGet(componentId, groupId, attribId);	/* See if attribute is valid */
	if (attrib != NULL){
		data = MIF_attributeDataGet(attrib, MIF_READ);
		if (data->dataType == MIF_COMPONENT_PATH_NAME) return data;
	}
	return NULL;
}
	


static void DH_getOrSetAttributes(DMI_GetAttributeReq_t _FAR *dmiCommand)
{
#define setCommand ((DMI_SetAttributeReq_t _FAR *)dmiCommand)

    DMI_UNSIGNED				respCount;
    char _FAR                   *MAXOFFSET;
	DMI_UNSIGNED				componentId;
    DMI_UNSIGNED           		command;
    DB_Attribute_t				*a;
    DB_Data_t					*d;
    DMI_STRING        			*s;
	SL_ErrorCode_t				errorCode;
    DMI_GetAttributeCnf_t _FAR	*r;
    DMI_UNSIGNED 				accessType;
    DMI_UNSIGNED				valueType;
    DMI_UNSIGNED				dataType;
    DMI_UNSIGNED	  			dataSize;
	unsigned int					maxDataSize;
    MIF_Id_t					attribId;
    MIF_Id_t					groupId;
	unsigned short				keyCount;
	DMI_GroupKeyData_t _FAR 	*groupKeys;
	DMI_STRING _FAR				*setValue = (DMI_STRING _FAR *)NULL;
	TM_Task_t					nextTask = TM_CALLBACK;
	unsigned int					accessMethod;
	unsigned short					fakeRow;			/* For getAttributeData */
	int				          lockStatus;
	boolean						directInterface = FALSE;

    MAXOFFSET = (char _FAR *)((char _FAR *)dmiCommand + dmiCommand->DmiMgmtCommand.iCmdLen);
	componentId = dmiCommand->iComponentId;

    respCount = dmiCommand -> DmiMgmtCommand.iCnfCount;

	if (respCount == dmiCommand->DmiMgmtCommand.iRequestCount)
		goto done;

/* Try to lock component */
	lockStatus = MIF_componentLockTestAndSet(componentId,
									dmiCommand->DmiMgmtCommand.iMgmtHandle);
	if (lockStatus == -1)
	{
		errorCode = DBERR_COMPONENT_NOT_FOUND;
		goto error;
	}

	if (!lockStatus)
	{
		nextTask = TM_DB;
		goto done;
	}

    command = dmiCommand -> DmiMgmtCommand.iCommand;

	if (command == DmiGetAttributeCmd)
	{
		groupId = dmiCommand->DmiGetAttributeList[respCount].iGroupId;
	    attribId = dmiCommand->DmiGetAttributeList[respCount].iAttributeId;
		keyCount = (unsigned int)dmiCommand->DmiGetAttributeList[respCount].iGroupKeyCount;
		groupKeys = (DMI_GroupKeyData_t _FAR *)((char _FAR *)dmiCommand +
					dmiCommand->DmiGetAttributeList[respCount].oGroupKeyList);
		accessMethod = MIF_READ;
	}
	else
	{
	    groupId = setCommand->DmiSetAttributeList[respCount].iGroupId;
	    attribId = setCommand->DmiSetAttributeList[respCount].iAttributeId;
		keyCount = (unsigned int)setCommand->DmiSetAttributeList[respCount].iGroupKeyCount;
		groupKeys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)dmiCommand +
					setCommand->DmiSetAttributeList[respCount].oGroupKeyList);
		setValue = (void _FAR *) ((char _FAR *)dmiCommand +
					setCommand->DmiSetAttributeList[respCount].oAttributeValue);
		accessMethod = MIF_WRITE;
	}

	if (!DH_checkKeys(componentId, groupId, keyCount, groupKeys,MAXOFFSET,&errorCode))
	{
		errorCode = DBERR_ILLEGAL_KEYS;
		goto error;
	}

	errorCode = DBERR_ATTRIBUTE_NOT_FOUND;
    if ((a = MIF_attributeGet(componentId, groupId, attribId)) == NULL)
		goto error;

    dataType = a->dataType;

    accessType = a -> accessType & MIF_ACCESS_MODE_MASK;
	
	if (accessType == MIF_UNSUPPORTED)
	{
		errorCode = DBERR_ATTRIBUTE_NOT_SUPPORTED;
		goto error;
	}
	if (accessType == MIF_UNKNOWN)
	{
		errorCode = DBERR_VALUE_UNKNOWN;
		goto error;
	}

    if ((command != DmiGetAttributeCmd) && (accessType == MIF_READ_ONLY))
	{
        errorCode = DBERR_ILLEGAL_TO_SET;
        goto error;
    }
	else if ((command == DmiGetAttributeCmd) && (accessType == MIF_WRITE_ONLY))
	{
		errorCode = DBERR_ILLEGAL_TO_GET;
		goto error;
	}

	if (MH_directLookup(componentId, groupId, attribId, 0) != NULL)
	{
		directInterface = TRUE;
		d = MIF_attributeDataGet(a, MIF_READ);
	}
	else
	{

/* NOTE: If a keyed attribute value is in an overlay, this function returns the data element corresponding to the keyed attribute, not the attribId */
/* requested.  When the overlay is called, it can figure out whether or not the key value is valid; obviously, we can't. */

		fakeRow = 0;
		d = DH_getAttributeData((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId, groupId, attribId, keyCount, groupKeys, DH_RETURN_THIS_ROW, &fakeRow);
		if (d == NULL){
    		errorCode = DBERR_ILLEGAL_KEYS;
 			goto error;
        }
	}

/* Have to do size/value in two parts due to data base buffering (we lose */
/* d when we get s).  Need type info from attrib struct (data has overlay info) */
	dataSize = DB_dataTypeSizeGet(dataType);

/* Returns same data type unless overlay */
    valueType = d->dataType;

	if (command == DmiGetAttributeCmd)
	{
		if (dataSize == 0)
	 	   dataSize = (unsigned int)d->currentSize;
	}
	else if (command == DmiSetAttributeCmd)
	{
		if (dataSize == 0)
	    	dataSize = setValue->length;
		
		maxDataSize = (unsigned int)d->maxSize;
        if((dataType == MIF_OCTETSTRING) || (dataType == MIF_DISPLAYSTRING))
            dataSize += sizeof(DMI_UNSIGNED);   /* remove the length of the length field */

		if (dataSize > maxDataSize)
		{
			errorCode = DBERR_VALUE_EXCEEDS_MAXSIZE;
			goto error;
		}
		if (valueType != MIF_COMPONENT_PATH_NAME)						/* Current size 0 */
			d->currentSize = dataSize;									/* for overlays */
	}

	if (!directInterface)
		s = MIF_dataValueGet(d, accessMethod);
	else
		s = NULL;
    
/* If attribute is in overlay, set up overlay task */
	if (valueType == MIF_COMPONENT_PATH_NAME || directInterface)
	{
		DH_buildInstrumentationVector((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId, groupId, attribId, dataType, s);
		return;
	}

	if (command == DmiGetAttributeCmd)
	{
		r = SL_buildResponse(dmiCommand, (DMI_UNSIGNED)dataSize, (char _FAR *)s);
		if (r == NULL)
			goto done;

		r->iGroupId = groupId;
		r->iAttributeId = attribId;
		r->iAttributeType = dataType;
	}
	else
	{		
		if (command == DmiSetAttributeCmd)
			memcpy(s, setValue, (unsigned int)dataSize);

		dmiCommand->DmiMgmtCommand.iCnfCount++;
	}

/*	if (dmiCommand->DmiMgmtCommand.iCnfCount != dmiCommand->DmiMgmtCommand.iRequestCount) */
	nextTask = TM_DB;

	goto done;

error:
	SL_buildError(dmiCommand, errorCode);

done:
	TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *) dmiCommand, 0);

#undef setCommand
}


/* returns FALSE if getRow should set no next task since we may set task */
/* of TM_COMPONENT, or TM_DIRECT */
static boolean DH_getKeys(DMI_GetRowReq_t _FAR *dmiCommand, MIF_Id_t componentId, MIF_Id_t groupId, unsigned int *row)
{
	MIF_Id_t					attribId;
    DMI_GetRowCnf_t _FAR		*resp;
    DB_Data_t					*data = (DB_Data_t *)NULL;
	SL_ErrorCode_t				errorCode;
	unsigned short				keyCount;
	DMI_GroupKeyData_t _FAR		*groupKeys;
	DMI_GroupKeyData_t _FAR		*newKeys;
	DB_Key_t					*keys;
    int                         keySize = 0;
	DMI_UNSIGNED				command = dmiCommand->DmiMgmtCommand.iCommand;
    MIF_DataType_t				dataType;
	unsigned int					i;
	boolean						directInterface = FALSE;
    char _FAR                   *MAXOFFSET;

    MAXOFFSET = (char _FAR *)((char _FAR *)dmiCommand + dmiCommand->DmiMgmtCommand.iCmdLen);
	keys = MIF_keyFind(componentId, groupId);
	keyCount = keys == NULL ? 0 : (unsigned int)keys->keyCount;

	groupKeys = (DMI_GroupKeyData_t _FAR *)((char _FAR *)dmiCommand +
													dmiCommand->oGroupKeyList);
	
/* Group response not built yet? */
	if (*(int _FAR *)dmiCommand->DmiMgmtCommand.pCnfBuf == 0)
	{
/* GET_FIRST ignores any keys -- we return first group's keys from MIF */
		if (command != DmiGetFirstRowCmd)
		{
			errorCode = DBERR_ILLEGAL_KEYS;
			keyCount = (unsigned int)dmiCommand->iGroupKeyCount;
			if (keyCount == 0 && command == DmiGetNextRowCmd)
				goto error;

			if (!DH_checkKeys(componentId, groupId, keyCount, groupKeys,MAXOFFSET,&errorCode))
				goto error;
		}
		else
/* Used as flag to know when to stop getting keys */
			dmiCommand->iGroupKeyCount = (DMI_UNSIGNED)keyCount;

/* Must reserve space for all keys when building group response buf */
		if ((resp = SL_buildResponse(dmiCommand, (DMI_UNSIGNED)keyCount *
										sizeof(DMI_GroupKeyData_t), NULL)) == NULL)
			goto noCallback;				/* error set by buildResponse */

		dmiCommand->DmiMgmtCommand.iCnfCount--;	/* Don't incr until keys done */
	    resp->iGroupId = groupId;
	    resp->iAttributeCount = 0;
		resp->iGroupKeyCount = 0;		/* Initial value */

/* If get row, just copy the command's to response's keys; we're done */
		if (command == (unsigned int)DmiGetRowCmd)
		{
/* Return same group key list as we were given */
			for (i = 0; i < keyCount; i++)
			{
            	keySize = DB_dataTypeSizeGet((MIF_DataType_t)
                		                         groupKeys[i].iAttributeType);
    	        if (keySize == 0)
        	        keySize = (int) (((char _FAR *)dmiCommand)[groupKeys[i].oKeyValue]) + sizeof(DMI_UNSIGNED);
            
				newKeys = SL_buildGroupKey(dmiCommand, keySize,
						(char _FAR *)(dmiCommand) + groupKeys[i].oKeyValue);
				if (newKeys == NULL)
					goto noCallback;

				newKeys->iAttributeId = groupKeys[i].iAttributeId;
				newKeys->iAttributeType = groupKeys[i].iAttributeType;
			}
			resp->iGroupKeyCount = keyCount;
			return TRUE;
		}
	}
	else
		resp = dmiCommand->DmiMgmtCommand.pCnfBuf;

/* Note: If no keys defined for group, either request has groupKeyCount of 0 */
/* so we return true here, or groupKeyCount is not 0, in which case it is */
/* caught above (checkKeys) and an error is returned.  We don't need to  */
/* check for keys == NULL below. */

/* Can't copy keys from response to request since request doesn't have */
/* space for them! */
	if (resp->iGroupKeyCount == dmiCommand->iGroupKeyCount)
		return TRUE;

/* Get value for next key. The keys struct has already been verified.   */
	keys = MIF_keyFind(componentId, groupId);
	attribId = keys->keys[resp->iGroupKeyCount];

/* See if high perf handles attr; may between old attrib and new attrib */
	if (MH_directLookup(componentId, groupId, attribId,0) != NULL)
		directInterface = TRUE;
	else
	{
		errorCode = DBERR_ROW_NOT_FOUND;
		data = DH_getAttributeData((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId,groupId, attribId, keyCount, groupKeys, DH_RETURN_NEXT_ROW, (unsigned short *)row);
		if (data == NULL)
			goto error;
	}

/* If overlay, that will fill in the key */
    dataType = 0;
    if(data != (DB_Data_t *)NULL) dataType = data->dataType;

	if (directInterface || (dataType == MIF_COMPONENT_PATH_NAME))
	{
		DMI_STRING		*symbolName,*SymbolNameHold;
		DB_Attribute_t	*attrib;

		if (directInterface)
			symbolName = SymbolNameHold = NULL; /* preset these to NULL */
		else{
			symbolName = MIF_dataValueGet(data, MIF_READ);
            SymbolNameHold = OS_Alloc(stringLen(symbolName));
            if (SymbolNameHold == NULL){
                errorCode = SLERR_OUT_OF_MEMORY;
                goto error;
            }
            stringCpy(SymbolNameHold,symbolName);  /* copy it to a safe place for now */
 
        }
/* Find data type */
		attrib = MIF_attributeGet(componentId, groupId, attribId);
		newKeys = SL_buildGroupKey(dmiCommand, keySize, MIF_dataValueGet(data, MIF_READ));
		resp->iGroupKeyCount--;											/* buildGK increments key count, but value not filled in yet */
		newKeys->iAttributeId = attribId;
		newKeys->iAttributeType = attrib->dataType;
		DH_buildInstrumentationVector((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId, groupId, attribId, 0, SymbolNameHold);
        if(SymbolNameHold != (DMI_STRING *)NULL) OS_Free(SymbolNameHold);
		goto noCallback;												/* So we don't set a new nextTask */
	}

	keySize = DB_dataTypeSizeGet(data->dataType);
	if (keySize == 0)
		keySize = (unsigned int)data->currentSize + sizeof(DMI_UNSIGNED);

	newKeys = SL_buildGroupKey(dmiCommand, keySize, MIF_dataValueGet(data, MIF_READ));
	newKeys->iAttributeId = attribId;
	newKeys->iAttributeType = dataType;

	return TRUE;

error:
	SL_buildError(dmiCommand, errorCode);
	TM_taskSwitch(TM_CALLBACK, (DMI_MgmtCommand_t _FAR *)dmiCommand, NULL);
noCallback:
	return FALSE;
}


static void DH_getRow(DMI_GetRowReq_t _FAR *dmiCommand, unsigned int row)
{
    DMI_UNSIGNED				respCount;
    MIF_Id_t					componentId;
    MIF_Id_t					groupId;
    MIF_Id_t					attribId;
	DMI_UNSIGNED				accessType,Count;
    DB_Attribute_t				*a = NULL;
    DMI_GetRowCnf_t _FAR		*r1;
    DB_Data_t					*d;
    MIF_DataType_t				dataType;
    MIF_DataType_t				valueType;
    DMI_UNSIGNED				dataSize;
    DMI_STRING					*s;
    DMI_GetAttributeCnf_t _FAR	*r2;
	DMI_GroupKeyData_t _FAR		*groupKeys;
	SL_ErrorCode_t				errorCode;
	TM_Task_t  					nextTask = TM_DB;
	int				lockStatus;
	boolean						directInterface = FALSE;

/* NOTE:  If the previous get could not fit all attributes into the buffer */
/* the mgmt app will call with attrib Id of the last attrib that did fit. */
/* The first get will have an attribute Id of 0. */

    respCount = dmiCommand -> DmiMgmtCommand.iCnfCount;
    componentId = dmiCommand -> iComponentId;
    groupId = dmiCommand -> iGroupId;
	attribId = dmiCommand->iAttributeId;

/* Try to lock component */
	lockStatus = MIF_componentLockTestAndSet(componentId,
									dmiCommand->DmiMgmtCommand.iMgmtHandle);
	if (lockStatus == -1)
	{
		errorCode = DBERR_COMPONENT_NOT_FOUND;
		goto error;
	}

	if (!lockStatus)
		goto done;

/* In case getting more from this group */
	if(respCount == 0)
		if (!DH_getKeys(dmiCommand, componentId, groupId, &row))
			return;							/* Error and task already set */

	r1 = dmiCommand->DmiMgmtCommand.pCnfBuf;

/* Still putting keys in? */
	if (attribId == 0 && r1->iGroupKeyCount != dmiCommand->iGroupKeyCount)
		goto done;

	dmiCommand->DmiMgmtCommand.iCnfCount = 1;	/* Only 1 response ever generated */
	nextTask = TM_CALLBACK;

	errorCode = DBERR_ROW_NOT_FOUND;	/* Default error */

	for (Count = 0; ;Count++)
	{
		if ((a = MIF_attributeFindNext(componentId, groupId, attribId)) == NULL)
		{
			if((dmiCommand->iAttributeId == 0) ||
               ((Count == 0) && (respCount == 0))) goto error;
			goto done;
		}
		attribId = a->id;
    	accessType = a->accessType & MIF_ACCESS_MODE_MASK;
		if (accessType != MIF_UNSUPPORTED)								/* Treat as if not there if unsupported */
			break;
	}

	dataType = a->dataType;												/* Need for buildInstr */

/* See if high perf handles attr */
	if (MH_directLookup(componentId, groupId, attribId, 0) != NULL)
	{
		directInterface = TRUE;
		d = MIF_attributeDataGet(a, MIF_READ);
	}
	else
	{
		groupKeys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)dmiCommand + dmiCommand->oGroupKeyList);

		d = DH_getAttributeData((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId, groupId, attribId, (unsigned short)dmiCommand->iGroupKeyCount,
						groupKeys, ((DMI_MgmtCommand_t _FAR *)dmiCommand)->iCommand == DmiGetRowCmd ? DH_RETURN_THIS_ROW : DH_RETURN_NEXT_ROW, (unsigned short *)&row);

		if (d == NULL)
			goto error;
	}

    valueType = d->dataType;

	dataSize = DB_dataTypeSizeGet(valueType);
	if (dataSize == 0)													/* String */
		dataSize = d->currentSize + sizeof(DMI_UNSIGNED);

	if (accessType == MIF_WRITE_ONLY)
	{
		dataSize = 0;
		s = NULL;
	}
	else if (directInterface)
		s = NULL;
	else
		s = MIF_dataValueGet(d, MIF_READ);

	if ((accessType != MIF_WRITE_ONLY) && (valueType == MIF_COMPONENT_PATH_NAME || directInterface))
	{
		DH_buildInstrumentationVector((DMI_MgmtCommand_t _FAR *)dmiCommand, componentId, groupId, attribId, dataType, s);
		return;
	}

    if ((r2 = SL_buildGroupResponse(dmiCommand, dataSize, s)) == NULL)
		goto done;					/* Error set by buildGroupResponse */

	r2->iGroupId = groupId;
	r2->iAttributeId = attribId;
	r2->iAttributeType = dataType;
	dmiCommand->iAttributeId = attribId;		/* Cue for next time */
	if (accessType == MIF_WRITE_ONLY)
		r2->oAttributeValue = 0;

	nextTask = TM_DB;
	goto done;

error:
	SL_buildError(dmiCommand, errorCode);
	nextTask = TM_CALLBACK;

done:
    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)dmiCommand,
											(void _FAR *)(unsigned long)row);
}


static void DH_listAttributes(DMI_ListAttributeReq_t _FAR *lCommand)
{
    DMI_UNSIGNED				command;
    DMI_UNSIGNED				respCount;
    DMI_UNSIGNED				componentId;
    DMI_UNSIGNED				groupId;
    DMI_UNSIGNED				attribId;
    DMI_UNSIGNED				accessType;
    DMI_UNSIGNED				dataType;
    DMI_UNSIGNED				maxSize;
    DB_Attribute_t				*a = NULL;
    DB_Data_t					*d;
	MIF_Pos_t					enumPos;
	MIF_Pos_t					enumDataPos;
	DB_Enum_t					*e;
	DB_EnumElement_t			*enumElement;
	DMI_UNSIGNED				elementCount;
	DMI_EnumData_t _FAR			*enumList;
    DMI_STRING					*s;
    DMI_ListAttributeCnf_t _FAR	*r;
	SL_ErrorCode_t				errorCode = DBERR_ATTRIBUTE_NOT_FOUND;
	DMI_UNSIGNED				i;
	TM_Task_t	   				nextTask = TM_CALLBACK;

    command = lCommand -> DmiMgmtCommand.iCommand;
    respCount = lCommand -> DmiMgmtCommand.iCnfCount;
    componentId = lCommand -> iComponentId;
    groupId = lCommand -> iGroupId;

    if (respCount == 0)
		attribId = lCommand -> iAttributeId;
    else
		attribId = ((DMI_ListAttributeCnf_t _FAR *)
			       (lCommand -> DmiMgmtCommand.pCnfBuf))
		    	   [respCount - 1].iAttributeId;
    
	if (command == DmiListAttributeCmd)
		a = MIF_attributeGet(componentId, groupId, attribId);
    else
		a = MIF_attributeFindNext(componentId, groupId, attribId);

    if (a == NULL)
	{
		if (respCount == 0)
			goto error;

		goto done;
	}

	attribId = a -> id;
	accessType = a -> accessType;
	dataType = a -> dataType;

  	enumPos = a->enumLink;

	if (enumPos.page == 0)
		elementCount = 0;
	else
	{
		e = MIF_resolve(DB, enumPos, MIF_READ);
		elementCount = e->elementCount;
		enumPos = e->elementLink;
	}

	a = MIF_attributeGet(componentId, groupId, attribId);

	if ((d = MIF_attributeDataGet(a, MIF_READ)) == NULL)
		goto error;
    
	maxSize = d -> maxSize;
    
	if ((s = MIF_attributeNameGet(a)) == NULL)
		goto error;

/* Build attrib response and enum lists */
	if ((r = SL_buildResponse(lCommand, (DMI_UNSIGNED)stringLen(s),
							(DMI_STRING _FAR *)s,
							(DMI_UNSIGNED)elementCount * sizeof(DMI_EnumData_t),
							(void _FAR *)NULL)) == NULL)
	    goto done;						/* Error built by buildResponse */

	++respCount;   						/* Flag for error */
	r -> iAttributeId = attribId;
	r -> iAttributeAccess = accessType;
	r -> iAttributeType = dataType;
	r -> iAttributeMaxSize = maxSize;

    /* the following is only temporary until the database engine is fixed... */
	if (dataType == MIF_STRING || dataType == MIF_OCTETSTRING)
		r->iAttributeMaxSize -= sizeof(s->length);

	r -> iEnumListCount = 0;			/* Must be initialized for build */

	errorCode = DBERR_ENUM_ERROR;

	for (i = 0; i < elementCount; i++)
	{
		enumElement = MIF_resolve(DB, enumPos, MIF_READ);
		enumPos = enumElement->elementLinkNext;
		enumDataPos = enumElement->dataPos;

		if ((s = MIF_resolve(DB, enumElement->symbolPos, MIF_READ)) == NULL)
			goto error;

		if ((enumList = SL_buildEnumList(lCommand, s)) == NULL)
			goto done;
		
		d = MIF_resolve(DB, enumDataPos, MIF_READ);
		enumList->iEnumValue = *(DMI_UNSIGNED *)MIF_resolve(DB, d->valuePos,
															MIF_READ);
	}

	lCommand->iAttributeId = attribId;	   		/* So list next works */

	if (command != DmiListAttributeCmd)
		nextTask = TM_DB;

	goto done;

error:
	SL_buildError(lCommand, errorCode);

done:
    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)lCommand, 0);
}


/* Class strings are of the form "a|b|c..." */
static boolean DH_classCmp(DMI_STRING *class, DMI_STRING _FAR *filter)
{
	char		*classStr, _FAR *filterStr;
	unsigned int  classLen, filterLen;
	unsigned int	classIndex, filterIndex;

	if (class == NULL)
		return FALSE;

	for (classIndex = filterIndex = 0; ; )
	{
		classStr = &class->body[classIndex];
		filterStr = &filter->body[filterIndex];

		for (classLen = 0; classIndex < class->length; classIndex++, classLen++)
		{
			if (classStr[classLen] == '|')
			{
				classIndex++;				/* Point to after the '|' */
				break;
			}
		}

		if (classLen == 0)					/* All substrings compared? */
			return TRUE;

		for (filterLen = 0; filterIndex < filter->length; filterIndex++, filterLen++)
		{
			if (filterStr[filterLen] == '|')
			{
				filterIndex++;				/* Point to after the '|' */
				break;
			}
		}

		/* Filter length of 0 means anything matches */
		if (filterLen != 0)
		{
			if (classLen != filterLen)
				return FALSE;

			if (memcmp(classStr, filterStr, classLen) != 0)
				return FALSE;
		}
	}
}



static SL_ErrorCode_t DH_buildClassList(DMI_ListComponentReq_t _FAR *lCommand,
														DMI_UNSIGNED componentId)
{
	void _FAR						*respBuf;
	DB_Group_t 						*group;
	DMI_UNSIGNED			 		groupId = 0;
	unsigned int 						classListSize = 0;
	DMI_ListComponentCnf_t _FAR		*listResponse;
	DMI_ClassNameData_t _FAR		*classDesc;
	int								avail;
	DMI_STRING						*classStr;
	char _FAR						*classRespStr;
	unsigned int 						classStrSize;

	respBuf = lCommand->DmiMgmtCommand.pCnfBuf;

/* Since the class list array hasn't been allocated yet, we need to find */
/* number of groups.  Even if a group doesn't have a class string, a */
/* struture will be built for it; osClassName will be 0. */

	while ((group = MIF_groupFindNext(componentId, groupId)) != NULL)
	{
		classListSize += sizeof(DMI_ClassNameData_t);
		groupId = group->id;
	}

	/* Response already built */
	listResponse = (DMI_ListComponentCnf_t _FAR *)respBuf +
									lCommand->DmiMgmtCommand.iCnfCount - 1;

	/* What's left is the space between end of list resp and class list */
	/* The class list offset is already built -- points to component name */
	avail = (int)listResponse->oClassNameList - ((char _FAR *)listResponse +
					sizeof(DMI_ListComponentCnf_t) - (char _FAR *)respBuf);

	if ((avail -= classListSize) < 0)
		return SLERR_BUFFER_FULL;

	listResponse->oClassNameList -= classListSize;

	classDesc = (DMI_ClassNameData_t _FAR *) ((char _FAR *)respBuf + listResponse->oClassNameList);
	
	/* Initial value of class string pointer is class desc array */
	classRespStr = (char _FAR *)classDesc;
				
	groupId = 0;
	while ((group = MIF_groupFindNext(componentId, groupId)) != NULL)
	{
		classDesc->iGroupId = groupId = group->id;

		if ((classStr = MIF_classStrGet(componentId, groupId)) == NULL)
			classDesc->osClassString = 0;
		else
		{
			classStrSize = (unsigned int)stringLen(classStr);

			if ((avail -= classStrSize) < 0)
				return SLERR_BUFFER_FULL;

			classRespStr -= classStrSize;
			memcpy(classRespStr, classStr, classStrSize);

			classDesc->osClassString = (char _FAR *)classRespStr -
													(char _FAR *)respBuf;

		}
		classDesc++;
		listResponse->iClassListCount++;
	}
    return SLERR_NO_ERROR;
}


TM_Task_t DH_listComponent(DMI_ListComponentReq_t _FAR *lCommand)
{
    DMI_UNSIGNED					command;
    DMI_UNSIGNED					respCount;
    DMI_UNSIGNED					componentId;
    DB_Component_t					*c = NULL;
    DMI_STRING						*s;
    DMI_ListComponentCnf_t _FAR		*r;
	SL_ErrorCode_t					errorCode = DBERR_COMPONENT_NOT_FOUND;
	unsigned int						matchType;
	TM_Task_t 						nextTask = TM_CALLBACK;
    char _FAR                       *MAXOFFSET;

    MAXOFFSET = (char _FAR *)((char _FAR *)lCommand + lCommand->DmiMgmtCommand.iCmdLen);
    command = lCommand -> DmiMgmtCommand.iCommand;
    respCount = lCommand -> DmiMgmtCommand.iCnfCount;
    if (respCount == 0)
		componentId = lCommand -> iComponentId;
    else
		componentId = ((DMI_ListComponentCnf_t _FAR *)
			      				(lCommand -> DmiMgmtCommand.pCnfBuf))
		    		  			[respCount - 1].iComponentId;

	matchType = MIF_LIST_COMPONENT_MATCH;		/* Default */
/* NOTE: If no filtering, keys aren't checked either */
    if (command == DmiListComponentCmd)
	{
		c = MIF_componentGet(componentId);
		goto build_response;
	}
	 
	/* Use component that matches filtering.  Filtering is based on */
	/* class name and key values */
	for (;;)
	{
		DB_Group_t					*group;
		DMI_UNSIGNED   				groupId = 0;
		DB_Attribute_t				*attrib;
		DMI_STRING				*classStr;
		DB_Data_t					*data;
		unsigned short				keyCount;
		DMI_GroupKeyData_t _FAR	*filterKeys;
		unsigned short					fakeRow;

		c = MIF_componentFindNext(componentId);
		if (c == NULL)
			goto build_response;

		componentId = c -> id;

		if(lCommand->osClassString == 0){  /* no class string, is this really an error here ? */
            if(lCommand->iGroupKeyCount == 0) goto build_response;  /* no error, there is no filter here */
            errorCode = SLERR_ILL_FORMED_COMMAND;
            goto error;
        }
        if(((char _FAR *)lCommand + lCommand->osClassString) >= MAXOFFSET){
            errorCode = SLERR_ILL_FORMED_COMMAND;
			goto error;
        }

		while ((group = MIF_groupFindNext(componentId, groupId)) != NULL)
		{
			groupId = group->id;

			classStr = MIF_classStrGet(componentId, groupId);

/* Try to match class string for group; if not, go to next group */
			if (DH_classCmp(classStr, (DMI_STRING _FAR *) ((char _FAR *)lCommand + lCommand->osClassString)) )
			{
				keyCount = (unsigned int)lCommand->iGroupKeyCount;
				filterKeys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)lCommand + lCommand->oGroupKeyList);
				if (!DH_checkKeys(componentId, groupId, keyCount, filterKeys,MAXOFFSET,&errorCode))
					break;

				/* Get first attrib for getAttribData */
				attrib = MIF_attributeFindNext(componentId, groupId, 0);

				/* Use getAttribData to see if keys match; we don't use data */
				fakeRow = 0;
				if ((data = DH_getAttributeData((DMI_MgmtCommand_t _FAR *)lCommand,
					componentId, groupId, attrib->id,
					keyCount, filterKeys, DH_RETURN_THIS_ROW, &fakeRow))
					== NULL)

					continue;

				/* Either a match found or a key attrib is in overlay */
				if (data->dataType == MIF_COMPONENT_PATH_NAME)
					matchType = MIF_LIST_COMPONENT_POSSIBLE_MATCH;

				c = MIF_componentGet(componentId);		/* Get again! */
				goto build_response;
			}
		}
	}

build_response:

    if(errorCode == SLERR_ILL_FORMED_COMMAND) goto error;
    if (c == NULL)
	{
		if (respCount == 0)
			goto error;

		goto done;
	}

	componentId = c -> id;
	if ((s = MIF_componentNameGet(c)) == NULL)
		goto error;

   	r = SL_buildResponse(lCommand, (DMI_UNSIGNED)stringLen(s), (char _FAR *)s,
								(DMI_UNSIGNED)0, (void _FAR *)NULL, 
								(DMI_UNSIGNED)0, (void _FAR *)NULL);

    if (r == NULL)
		goto done;						/* Error built by buildResponse */

	++respCount;
	r->iComponentId = componentId;
	r->iClassListCount = 0;
	r->iFileCount = 0;
	r->iMatchType = matchType;

	if ((errorCode = DH_buildClassList(lCommand, componentId)) != SLERR_NO_ERROR){
        (lCommand -> DmiMgmtCommand.iCnfCount)--;   /* correct the confirm count here */
        respCount--;
        errorCode = SLERR_NO_ERROR_MORE_DATA;       /* there are more components to get here */
		goto error;
    }
	lCommand->iComponentId = componentId;								/* So list next works */

	if (command != DmiListComponentCmd)
		nextTask = TM_DB;

	goto done;

error:
    SL_buildError(lCommand, errorCode);

done:
	return nextTask;
}


static void DH_listComponents(DMI_ListComponentReq_t _FAR *dmiCommand)
{
	TM_Task_t 		nextTask;

	nextTask = DH_listComponent(dmiCommand);

    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)dmiCommand, 0);
}


static void DH_listGroups(DMI_ListGroupReq_t _FAR *lCommand)
{
	void _FAR				*respBuf;
    DMI_UNSIGNED			command;
    DMI_UNSIGNED			respCount;
    DMI_UNSIGNED			componentId;
    DMI_UNSIGNED			groupId;
    DB_Group_t				*g = NULL;
	DB_Key_t				*k;
    DMI_STRING				*s;
    DMI_ListGroupCnf_t _FAR	*r;
	DMI_GroupKeyData_t _FAR *keys;
	DB_Attribute_t			*a;
	unsigned int				keyCount;
	unsigned int				i;
	DMI_STRING _FAR			*classStr;
	unsigned int				classLen;
	TM_Task_t 				nextTask = TM_CALLBACK;

    command = lCommand -> DmiMgmtCommand.iCommand;
    respCount = lCommand -> DmiMgmtCommand.iCnfCount;
	respBuf = lCommand->DmiMgmtCommand.pCnfBuf;
    componentId = lCommand -> iComponentId;
    if (respCount == 0)
		groupId = lCommand -> iGroupId;
    else
		groupId = ((DMI_ListGroupCnf_t _FAR *)
			  (lCommand -> DmiMgmtCommand.pCnfBuf))[respCount - 1].iGroupId;

    if (command == DmiListGroupCmd)
		g = MIF_groupGet(componentId, groupId);
    else
		g = MIF_groupFindNext(componentId, groupId);

    if (g == NULL)
	{
		if (respCount == 0)
			goto error;

		goto done;
	}

	groupId = g->id;

	keyCount = 0;
	if ((k = MIF_keyFind(componentId, groupId)) != NULL)
		keyCount = (unsigned int)k->keyCount;

	if ((s = MIF_classStrGet(componentId, groupId)) == NULL)
		classLen = 0;
	else
		classLen = stringLen(s);

/* Have to get again! */
	g = MIF_groupGet(componentId, groupId);

	if ((s = MIF_groupNameGet(g)) == NULL)
		goto error;

/* Reserve space for keys, but no value */
    r = SL_buildResponse(lCommand, (DMI_UNSIGNED)stringLen(s),(char _FAR *)s,
								(DMI_UNSIGNED)classLen, (void _FAR *)NULL, 
								(DMI_UNSIGNED)keyCount * sizeof(DMI_GroupKeyData_t),
								(void _FAR *)NULL);
    if (r ==NULL)
	    goto done;

	r->iGroupId = groupId;

/* Insert class string into group response */
	if (classLen != 0)
	{
		classStr = (DMI_STRING _FAR *) ((char _FAR *)respBuf + r->osClassString);
		s = MIF_classStrGet(componentId, groupId);
		memcpy(classStr, s, classLen);
	}
	else
		r->osClassString = 0;

/* Insert keys into group response */
	r->iGroupKeyCount = keyCount;
	
	keys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)respBuf + r->oGroupKeyList);

	for (i = 0; i < keyCount; i++)
	{
		k = MIF_keyFind(componentId, groupId);
		a = MIF_attributeGet(componentId, groupId, k->keys[i]);
		keys[i].iAttributeId = a->id;
		keys[i].iAttributeType = a->dataType;
		keys[i].oKeyValue = 0;
	}

	lCommand->iGroupId = groupId;			/* So list next works */

    if (command != DmiListGroupCmd)
		nextTask = TM_DB;

	goto done;

error:
    SL_buildError(lCommand, DBERR_GROUP_NOT_FOUND);

done:
    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)lCommand, 0);
}


static void DH_listDescription(DMI_ListDescReq_t _FAR *dmiCommand)
{
	DMI_UNSIGNED		componentId;
	DMI_UNSIGNED		groupId;
	SL_ErrorCode_t		errorCode = DBERR_NO_DESCRIPTION;
	DMI_STRING 			*desc = (DMI_STRING *)NULL;
	unsigned int			avail;
	DMI_UNSIGNED		size;
	DMI_UNSIGNED		descOffset;
	DMI_STRING _FAR		*descResp;
	TM_Task_t  			nextTask = TM_CALLBACK;

	componentId = dmiCommand->iComponentId;
	groupId = dmiCommand->iGroupId;

	switch (dmiCommand->DmiMgmtCommand.iCommand)
	{
		case DmiListAttributeDescCmd:
			desc = MIF_attributeDescGet(componentId, groupId, dmiCommand->iAttributeId);
			break;

		case DmiListComponentDescCmd:
			desc = MIF_componentDescGet(componentId);
			break;

		case DmiListGroupDescCmd:
			desc = MIF_groupDescGet(componentId, groupId);
			break;
	}

	if (desc == NULL)
		goto done;

	errorCode = SLERR_NO_ERROR;

	descOffset = dmiCommand->iOffset;
	if (descOffset != 0)
		descOffset++;						/* Get to NEXT char */

	if (descOffset > (DMI_UNSIGNED)desc->length)
		goto done;

	avail = (unsigned int)(dmiCommand->DmiMgmtCommand.iCnfBufLen -
														sizeof(desc->length));

	/* NOTE: size = desc->length - (unsigned int)descOffset; doesn't work! */
	/* It gives a "conversion between different integral types" warning!! */
	size = desc->length;
	size -= (unsigned int)descOffset;

	if (size > (unsigned int)avail)
	{
		size = (unsigned int)avail;
		errorCode = SLERR_NO_ERROR_MORE_DATA;
	}

	descResp = (DMI_STRING _FAR *)dmiCommand->DmiMgmtCommand.pCnfBuf;
	descResp->length = size;
	memcpy(descResp->body, &desc->body[descOffset], (unsigned int) size);

	if (descOffset == 0)				/* First time, convert to offset */
		size--;

	dmiCommand->iOffset += size;
	dmiCommand->DmiMgmtCommand.iCnfCount = 1;

done:
	SL_buildError(dmiCommand, errorCode);

    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)dmiCommand, 0);
}

static void DH_ListGroupPragma(DMI_ListGroupPragmaReq_t _FAR *dmiCommand)
{
DMI_UNSIGNED        componentId;
DMI_UNSIGNED        groupId;
SL_ErrorCode_t      errorCode = DBERR_NO_PRAGMA;
DMI_STRING          *Pragma = (DMI_STRING *)NULL;
unsigned int        avail;
DMI_UNSIGNED        size;
DMI_UNSIGNED        pragmaOffset;
DMI_STRING _FAR     *pragmaResp;
TM_Task_t           nextTask = TM_CALLBACK;
DB_Group_t          *group;

    componentId = dmiCommand->iComponentId;
    groupId = dmiCommand->iGroupId;
    group = MIF_groupGet(componentId, groupId);
    if (group == NULL) goto done;
    if (group->pragma.page == 0) goto done;
    Pragma = (DMI_STRING *)MIF_resolve(DB, group->pragma, MIF_READ);
    if (Pragma == NULL) goto done;
    errorCode = SLERR_NO_ERROR;
    pragmaOffset = dmiCommand->iOffset;
    if (pragmaOffset != 0) pragmaOffset++;    /* Get to NEXT char */
    if (pragmaOffset > (DMI_UNSIGNED)Pragma->length) goto done;
    avail = (unsigned int)(dmiCommand->DmiMgmtCommand.iCnfBufLen - sizeof(Pragma->length));

    /* NOTE: size = desc->length - (unsigned int)descOffset; doesn't work! */
    /* It gives a "conversion between different integral types" warning!! */
    size = Pragma->length;
    size -= (unsigned int)pragmaOffset;

    if (size > (unsigned int)avail){
        size = (unsigned int)avail;
        errorCode = SLERR_NO_ERROR_MORE_DATA;
    }
    pragmaResp = (DMI_STRING _FAR *)dmiCommand->DmiMgmtCommand.pCnfBuf;
    pragmaResp->length = size;
    memcpy(pragmaResp->body, &Pragma->body[pragmaOffset], (unsigned int) size);
    if (pragmaOffset == 0) size--;    /* First time, convert to offset */
    dmiCommand->iOffset += size;
    dmiCommand->DmiMgmtCommand.iCnfCount = 1;

done:
    SL_buildError(dmiCommand, errorCode);
    TM_taskSwitch(nextTask, (DMI_MgmtCommand_t _FAR *)dmiCommand, 0);
}

