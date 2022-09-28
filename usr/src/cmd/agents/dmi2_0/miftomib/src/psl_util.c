/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_util.c	1.3 96/09/24 Sun Microsystems"


/*****************************************************************************************************************************************************

    Filename: psl_util.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: Service Layer C-code Utilities

    Author(s): Alvin I. PIvowar, Paul A. Ruocchio

    RCS Revision: $Header: n:/sl/rcs/psl_util.c 1.75 1994/05/18 09:42:14 shanraha Exp $

    Revision History:

        Date     	Author Description
        -------  	---    -----------------------------------------------
        3/30/93  	aip		Creation date.
		10/14/93 	sfh		Fix buildEnumList to call buildError when buffer full.
		10/26/93 	sfh		Fix buildEnumList to decrement iCnfCount when buffer full.
							Remove references to variable 'i' in copyResponse.
		10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
		10/29/93 	sfh		Convert unsignedToAscii to C.
		11/02/93 	sfh		Move unsignedToAscii back to slutils.asm.  Not used in PSL.
		11/11/93 	sfh		Include psl.h.
		12/08/93 	sfh		Change osMapFileNames to osMapFiles.
		12/21/93 	sfh		Add _cdecl modifier to buildResponse so can build with pascal linkage.
		01/05/94 	sfh		Set oCmdListEntry to 0 for GetRow commands.
		01/21/94 	sfh		Change avail declaration to int in buildGroupResponseOrKey.
		01/25/94 	sfh		Fix buildGroupResponseOrKey to not increment confirm count in dmiCommand.
		03/04/94	sfh		Add parent/child code.
		03/23/94	sfh		Convert fstring functions to string.
		04/07/94	sfh		Move DMI_STRING functions from old db_strng.c.
		04/08/94	sfh		Add SL_buildPathname.
		04/19/94 	sfh		Remove include of slutils.h.  Not a PSL file.
		04/20/94	sfh		Rename oFileList to DmiFileList.
		05/10/94	sfh		Update to 1.0 (draft 4.6).
								- Change osClassName to osClassString.
		05/18/94	sfh		Revise buildCiCommand to return GetAttributeCnf pointer.
        05/20/94    par         Modified for use with PSL version 2.0
        06/18/94    par         changed name to conform to new naming rules
        07/23/94    par         Modified for new *.h files

****************************************************************************************************************************************************/


/****************************************************************** INCLUDES ***********************************************************************/

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "os_dmi.h"
#include "error.h"
#include "psl_main.h"
#include "db_api.h"
#include "psl_util.h"
#include "os_svc.h"

/***************************************************************************************************************************************************/


/******************************************************************* DEFINES ***********************************************************************/

#ifndef min
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

/***************************************************************************************************************************************************/


/******************************************************************** TYPEDEFS *********************************************************************/

/***************************************************************************************************************************************************/

		
/********************************************************************** DATA ***********************************************************************/

SL_ResponseTable_t GetAttribResponse =
		{sizeof(DMI_GetAttributeCnf_t),
		1,
		offsetof(DMI_GetAttributeCnf_t, oAttributeValue)};

SL_ResponseTable_t GetGroupResponse =
		{sizeof(DMI_GetRowCnf_t),
		1,
		offsetof(DMI_GetRowCnf_t, oGroupKeyList)};

SL_ResponseTable_t ListAttribResponse =
		{sizeof(DMI_ListAttributeCnf_t),
		2,
		offsetof(DMI_ListAttributeCnf_t, osAttributeName),
		offsetof(DMI_ListAttributeCnf_t, oEnumList)};

SL_ResponseTable_t ListComponentResponse =
		{sizeof(DMI_ListComponentCnf_t),
		2,
		offsetof(DMI_ListComponentCnf_t, osComponentName),
		offsetof(DMI_ListComponentCnf_t, oClassNameList)};

SL_ResponseTable_t ListGroupCnfonse =
		{sizeof(DMI_ListGroupCnf_t),
		3,
		offsetof(DMI_ListGroupCnf_t, osGroupName),
		offsetof(DMI_ListGroupCnf_t, osClassString),
		offsetof(DMI_ListGroupCnf_t, oGroupKeyList)};

SL_ResponseTable_t RegisterResponse =
		{sizeof(DMI_RegisterCnf_t),
		2,
		offsetof(DMI_RegisterCnf_t, DmiVersion.osDmiSpecLevel),
		offsetof(DMI_RegisterCnf_t, DmiVersion.osImplDesc)};

extern char    *SL_path;                       /* Path sl is in.  Allocated and initialized at startup. */

/***************************************************************************************************************************************************/



void SL_buildError(void _FAR *dmiCommand, SL_ErrorCode_t errorCode)
{
	((DMI_MgmtCommand_t _FAR *)dmiCommand)->iStatus = errorCode;
}


/* Assumes data built from bottom up                                         */
DMI_GetAttributeCnf_t _FAR *SL_buildCiCommand(void _FAR *dmiCommand)
{
#define cmd ((DMI_MgmtCommand_t _FAR *)dmiCommand)
#define gRespBufPtr ((DMI_GetRowCnf_t _FAR *)respBufPtr)

	unsigned short					command;
	unsigned short					respCount;
	DMI_GetAttributeCnf_t _FAR	*nextAttrResp;
	void _FAR					*respBufPtr;
	DMI_GroupKeyData_t _FAR 	*keys;
	unsigned short					argOffset;
	unsigned short					respOffset;
	unsigned short					i;
	unsigned short					avail;

	command = (unsigned short)cmd->iCommand;
									  
	if (command == DmiGetFirstRowCmd || command == DmiGetNextRowCmd)
		command = DmiGetRowCmd;

	respBufPtr = cmd->pCnfBuf;
	argOffset = (unsigned short)cmd->iCnfBufLen;

	if (command == DmiGetAttributeCmd || command == DmiGetRowCmd)
	{
		if (command == DmiGetRowCmd)
		{
			argOffset = (unsigned short)gRespBufPtr->oGroupKeyList;
			keys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)respBufPtr + argOffset);
			for (i = 0; i < (unsigned short)gRespBufPtr->iGroupKeyCount; i++)
				argOffset = min(argOffset, (unsigned short)keys[i].oKeyValue);

			respOffset = offsetof(DMI_GetRowCnf_t, DmiGetAttributeList);
			respCount = (unsigned short)gRespBufPtr->iAttributeCount;
		}
		else
		{
			respOffset = 0;
			respCount = (unsigned short)cmd->iCnfCount;
		}

		nextAttrResp = (DMI_GetAttributeCnf_t _FAR *) ((char _FAR *)respBufPtr + respOffset);

		for (i = 0; i < respCount; i++)
			argOffset = min(argOffset, (unsigned short)nextAttrResp[i].oAttributeValue);

		respOffset += respCount * sizeof(DMI_GetAttributeCnf_t);

		avail = argOffset - respOffset;

/* See if enough space for one get attr response + one int. Not a perfect test. */
		if (argOffset <= respOffset)			  /* new test that checks for decrement */
		{                         				  /* below logical zero                 */
			SL_buildError(dmiCommand, respCount == 0 ? SLERR_BUFFER_FULL : SLERR_NO_ERROR_MORE_DATA); 
			return FALSE;    
		} 

		if (command == DmiGetRowCmd)
			cmd->DmiCiCommand.oCmdListEntry = 0;
		else
			cmd->DmiCiCommand.oCmdListEntry = offsetof(DMI_GetAttributeReq_t, DmiGetAttributeList) + cmd->iCnfCount * sizeof(DMI_GetAttributeData_t);
	
		cmd->DmiCiCommand.iCnfBufLen = avail;
		cmd->DmiCiCommand.pCnfBuf = (void _FAR *) ((char _FAR *)respBufPtr + respOffset);
	}

	else
	{
		cmd->DmiCiCommand.oCmdListEntry = offsetof(DMI_SetAttributeReq_t, DmiSetAttributeList) + cmd->iCnfCount * sizeof(DMI_SetAttributeData_t);
		cmd->DmiCiCommand.iCnfBufLen = cmd->iCnfBufLen;
		cmd->DmiCiCommand.pCnfBuf = cmd->pCnfBuf;
	}

/* The confirm function is actually set in DOS by ol.asm                     */
	cmd->DmiCiCommand.pConfirmFunc = (DMI_FUNC3_OUT)OM_responseDone;

	return cmd->DmiCiCommand.pCnfBuf;

#undef cmd
#undef gRespBufPtr
}
 


/* Variable arguments are DMI_UNSIGNED, _FAR *, ...                          */
void _FAR * SL_buildResponse(void _FAR *dmiCommand, ...)
{
	SL_ResponseTable_t	*response = (SL_ResponseTable_t *)NULL;
	unsigned short			command;
	unsigned short 			*offsets;
    va_list						argPtr;
    char _FAR					*respBuf;
    DMI_UNSIGNED				respCount;
    unsigned short				structSize;
    unsigned short				offsetCount;
    DMI_OFFSET					argOffset;
    char _FAR					*structPtr;
    short							avail;
    unsigned short					i;
    unsigned short					size;
	char _FAR					*value;
	char _FAR					*respValue;
	DMI_EnumData_t _FAR			*enumList;
	SL_ErrorCode_t				errorCode;
	DMI_ClassNameData_t _FAR	*classList;

	command = (unsigned short)((DMI_MgmtCommand_t _FAR *)dmiCommand)->iCommand;

	switch (command)
	{
		case DmiRegisterMgmtCmd:
		case DmiRegisterCiCmd:
			response = &RegisterResponse;
			break;

		case DmiListComponentCmd:
		case DmiListFirstComponentCmd:
		case DmiListNextComponentCmd:
			response = &ListComponentResponse;
			break;

		case DmiListGroupCmd:
		case DmiListFirstGroupCmd:
		case DmiListNextGroupCmd:
			response = &ListGroupCnfonse;
			break;

		case DmiListAttributeCmd:
		case DmiListFirstAttributeCmd:
		case DmiListNextAttributeCmd:
			response = &ListAttribResponse;
			break;

		case DmiGetAttributeCmd:
			response = &GetAttribResponse;
			break;

		case DmiGetRowCmd:
		case DmiGetFirstRowCmd:
		case DmiGetNextRowCmd:
			response = &GetGroupResponse;
			break;

	}

	structSize = response->structSize;
	offsetCount = response->offsetCount;
	offsets = response->offsets;

    respBuf = ((DMI_MgmtCommand_t _FAR *) dmiCommand) -> pCnfBuf;
    respCount = ((DMI_MgmtCommand_t _FAR *) dmiCommand) -> iCnfCount;

/* Pointer to new response struct                                            */
    structPtr = respBuf + (respCount * structSize);

    if (respCount == 0)
        argOffset = (((DMI_MgmtCommand_t _FAR *) dmiCommand) -> iCnfBufLen);
    else
	{
        argOffset = *(DMI_OFFSET _FAR *)((char _FAR *)structPtr -
						structSize + offsets[offsetCount - 1]);

		if (command == DmiGetAttributeCmd)
		{		
			for (i = 0; i < respCount; i++)
				argOffset = min((unsigned short)argOffset,
					(unsigned short)((DMI_GetAttributeCnf_t _FAR *)respBuf)[i].oAttributeValue);
		}

		else if (command == DmiListAttributeCmd || command == DmiListFirstAttributeCmd ||
						command == DmiListNextAttributeCmd)
		{
			enumList =  (DMI_EnumData_t _FAR *)((char _FAR *)respBuf + argOffset)
						+ ((DMI_ListAttributeCnf_t _FAR *)structPtr)[-1].iEnumListCount;

			if (((DMI_ListAttributeCnf_t _FAR *)structPtr)[-1].iEnumListCount > 0)
				argOffset = enumList[-1].osEnumName;
		}

        else if (command == DmiListComponentCmd ||
				command == DmiListFirstComponentCmd ||
	          	command == DmiListNextComponentCmd)
		{
            classList = (DMI_ClassNameData_t _FAR *)((char _FAR *)respBuf +
				((DMI_ListComponentCnf_t _FAR *)structPtr)[-1].oClassNameList);

/* Since groups with no class names have offset 0, we need to check each one */
/* May need to rework list component when mapfiles are used                  */
			for (i = 0;	i < (unsigned short)((DMI_ListComponentCnf_t _FAR *)
						structPtr)[-1].iClassListCount; i++)
				if (classList[i].osClassString != 0)
	                argOffset = classList[i].osClassString;
        }
	}

    avail = respBuf + argOffset - structPtr;

	errorCode = respCount == 0 ? SLERR_BUFFER_FULL : SLERR_NO_ERROR_MORE_DATA;

    avail -= structSize;
    if (avail < 0)
		goto error;

    va_start(argPtr, dmiCommand);

    for (i = 0; i < offsetCount; ++i)
	{
        size = (unsigned short)va_arg(argPtr, DMI_UNSIGNED);
		size = alignmem(size); 
        avail -= size;
        if (avail < 0)
			goto error;

        argOffset -= size;
        *(DMI_OFFSET _FAR *)((char _FAR *)structPtr + offsets[i]) =
																argOffset;
		if ((value = va_arg(argPtr, char _FAR *)) != NULL)
		{
			respValue = respBuf + argOffset;
			memcpy(respValue, value, size);
		}
    }
    ++ (((DMI_MgmtCommand_t _FAR *) dmiCommand) -> iCnfCount);
    return structPtr;

error:
    SL_buildError(dmiCommand, errorCode);
    return NULL;

}


/* Call with attribOrKey = 1 for attrib, 0 for key                           */
static void _FAR *SL_buildGroupResponseOrKey(DMI_GetRowReq_t _FAR *dmiCommand,
							unsigned short attribOrKey,
							DMI_UNSIGNED size, void _FAR *value)
{
	DMI_GetRowCnf_t _FAR		*respBuf;
	DMI_UNSIGNED	     		respCount;
	DMI_UNSIGNED 				sizeNeeded;
	char _FAR 					*structPtr;
	DMI_OFFSET					argOffset;
	short							avail;
	DMI_GroupKeyData_t _FAR		*keyPtr;
	DMI_UNSIGNED 				keyCount;
	SL_ErrorCode_t				errorCode;
	unsigned short					offset;
	unsigned short					i;

    respBuf = (DMI_GetRowCnf_t _FAR *) dmiCommand -> DmiMgmtCommand.pCnfBuf;
    respCount = respBuf->iAttributeCount;
    sizeNeeded = size + (attribOrKey ? sizeof(DMI_GetAttributeCnf_t) : 0);

	argOffset = respBuf->oGroupKeyList;
	keyCount = respBuf->iGroupKeyCount;
	keyPtr = (DMI_GroupKeyData_t _FAR *)((char _FAR *)respBuf + argOffset);

    structPtr = (char _FAR *)respBuf + offsetof(DMI_GetRowCnf_t,
														DmiGetAttributeList);

/* If respCount != 0, attributes are being added                             */
	for (i = 0; i < (unsigned short)respCount; i++)
		argOffset = min((unsigned short)argOffset,
			(unsigned short)((DMI_GetAttributeCnf_t _FAR *)structPtr)[i].oAttributeValue);

	structPtr += respCount * sizeof(DMI_GetAttributeCnf_t);

/* Have to get offset from last key struct (if 0, this is already set)       */
/* We must use the smallest offset, since that is the last one used          */
	if (keyCount != 0)
	{
		keyPtr = keyPtr + keyCount;
		argOffset = min(argOffset, keyPtr[-1].oKeyValue);
	}
	 	
    avail = (char _FAR *)respBuf + argOffset - (char _FAR *)structPtr;
    avail -= (short)sizeNeeded;

	if (attribOrKey && respCount != 0)
		errorCode = SLERR_NO_ERROR_MORE_DATA;
	else
		errorCode = SLERR_BUFFER_FULL;

    if (avail < 0)
	{
		SL_buildError(dmiCommand, errorCode);
        return NULL;
    }

/* 1 = attrib, 0 = key                                                       */
	if (attribOrKey)
	{
		offset = offsetof(DMI_GetAttributeCnf_t, oAttributeValue);
	    ++(respBuf->iAttributeCount);
	}
	else
	{	   
		structPtr = (char _FAR *)keyPtr;
		offset = offsetof(DMI_GroupKeyData_t, oKeyValue);
		++(respBuf->iGroupKeyCount);
	}

    argOffset -= size;
	*(DMI_OFFSET _FAR *)((char _FAR *)structPtr + offset) = argOffset;
    memcpy((char _FAR *)respBuf + argOffset, value, (short)size);

	return structPtr;
}


DMI_GetAttributeCnf_t _FAR *SL_buildGroupResponse(DMI_GetRowReq_t _FAR *dmiCommand,
							DMI_UNSIGNED attribSize, void _FAR *attribValue)
{
	return (DMI_GetAttributeCnf_t _FAR *)SL_buildGroupResponseOrKey(
					dmiCommand, 1,attribSize, attribValue);
}


DMI_GroupKeyData_t _FAR *SL_buildGroupKey(DMI_GetRowReq_t _FAR *dmiCommand,
							DMI_UNSIGNED keySize, void _FAR *keyValue)
{
	return (DMI_GroupKeyData_t _FAR *)SL_buildGroupResponseOrKey(
					dmiCommand, 0, keySize, keyValue);
}

				
DMI_EnumData_t _FAR	*SL_buildEnumList(DMI_ListAttributeReq_t _FAR *cmd, DMI_STRING *enumValue)
{
	char _FAR				 	*respBuf;
	DMI_ListAttributeCnf_t _FAR	*listResp;
	DMI_OFFSET					argOffset;
	DMI_EnumData_t _FAR			*enumList;
	short							avail;
	unsigned short					responseCount;
	SL_ErrorCode_t				errorCode;

	responseCount = (unsigned short)cmd->DmiMgmtCommand.iCnfCount;

/* If cnf count is 1, we're in the middle of filling in cnf buf for 1st attr */
	errorCode = responseCount == 1 ? SLERR_BUFFER_FULL : SLERR_NO_ERROR_MORE_DATA;

	respBuf = ((DMI_MgmtCommand_t _FAR *)cmd)->pCnfBuf;
	listResp = (DMI_ListAttributeCnf_t _FAR *)respBuf +
						((DMI_MgmtCommand_t _FAR *)cmd)->iCnfCount - 1;
	
	argOffset = listResp->oEnumList;
	enumList =  (DMI_EnumData_t _FAR *)((char _FAR *)respBuf + argOffset)
					+ listResp->iEnumListCount;

	if (listResp->iEnumListCount > 0)
		argOffset = enumList[-1].osEnumName;

	argOffset -= stringLen(enumValue);
	avail = respBuf + argOffset - ((char _FAR *)listResp +
											sizeof(DMI_ListAttributeCnf_t));
	
	if (avail < 0)
	{
		--cmd->DmiMgmtCommand.iCnfCount;		/* This response not good                */
		SL_buildError(cmd, errorCode);
		return NULL;
	}

	memcpy(respBuf + argOffset, enumValue, stringLen(enumValue));
	enumList->osEnumName = argOffset;
	++listResp->iEnumListCount;

	return enumList;
}



/* This function assures that all data (not response structures) in the response */
/* buffer is packed into the bottom of the response buffer.  Note that for   */
/* group commands, the group key list is allocated at the bottom by buildResponse; */
/* however, the key values may have been inserted anywhere.                  */
void SL_packResponse(DMI_Confirm_t _FAR *ciConfirm)
{
#define ATTRIB_TYPE 0
#define KEY_TYPE 1

DMI_GetRowCnf_t _FAR		*groupResp;
DMI_GetAttributeCnf_t _FAR	*attribResp;
DMI_GroupKeyData_t _FAR	*groupKeys = (DMI_GroupKeyData_t _FAR *)NULL;
void _FAR					*value = (void _FAR *)NULL;
DMI_OFFSET	_FAR			*valueOffset = (DMI_OFFSET _FAR *)NULL;
unsigned short					command;
unsigned short					type = MIF_UNKNOWN_DATA_TYPE;
unsigned short					size;
unsigned short 					newOffset;
unsigned short 					oldOffset;
unsigned short					packOffset;
unsigned short  					i;
unsigned short  					respCount;
unsigned short 					offset;
DMI_MgmtCommand_t _FAR		*dmiCommand;

    dmiCommand = ciConfirm->pDmiMgmtCommand;
    if ((dmiCommand->iStatus = ciConfirm->iStatus) == SLERR_BUFFER_FULL)
        if (dmiCommand->iCnfCount != 0) dmiCommand->iStatus = SLERR_NO_ERROR_MORE_DATA;
    if (dmiCommand->iStatus != SLERR_NO_ERROR) return;
    command = (unsigned short)dmiCommand->iCommand;
    switch (command){
        case DmiSetAttributeCmd:
        case DmiSetReserveAttributeCmd:
        case DmiSetReleaseAttributeCmd:
            dmiCommand->iCnfCount++;
            return;
    }
    groupResp = (DMI_GetRowCnf_t _FAR *)dmiCommand->pCnfBuf;
    if (command != DmiGetAttributeCmd){
        oldOffset = packOffset = (unsigned short)groupResp->oGroupKeyList;
        groupKeys = (DMI_GroupKeyData_t _FAR *) ((char _FAR *)groupResp + packOffset);
        attribResp = (DMI_GetAttributeCnf_t _FAR *) ((char _FAR *)groupResp +
            offsetof(DMI_GetRowCnf_t, DmiGetAttributeList));
        respCount = (unsigned short)groupResp->iAttributeCount;
        if (respCount != 0)                                    
            ((DMI_GetRowReq_t _FAR *)dmiCommand)->iAttributeId = attribResp[respCount-1].iAttributeId;     
    }
    else{
        dmiCommand->iCnfCount++;
        attribResp = (DMI_GetAttributeCnf_t _FAR *)groupResp;
        oldOffset = packOffset = (unsigned short)dmiCommand->iCnfBufLen;
        respCount = (unsigned short)dmiCommand->iCnfCount;
    }
    for (;;){
        newOffset = 0;
        if (command != DmiGetAttributeCmd){
            for (i = 0; i < (unsigned short)groupResp->iGroupKeyCount; i++){
                offset = (unsigned short)groupKeys[i].oKeyValue;
                if (offset < oldOffset && offset > newOffset){
                    type = (unsigned short)groupKeys[i].iAttributeType;
                    value = (void _FAR *) ((char _FAR *)groupResp + groupKeys[i].oKeyValue);
                    valueOffset = &groupKeys[i].oKeyValue;
                    newOffset = offset;
                }
            }
        }
        for (i = 0; i < respCount; i++){
            offset = (unsigned short)attribResp[i].oAttributeValue;
            if (offset < oldOffset && offset > newOffset){
                type = (unsigned short)attribResp[i].iAttributeType;
                value = (void _FAR *) ((char _FAR *)groupResp + attribResp[i].oAttributeValue);
                valueOffset = &attribResp[i].oAttributeValue;
                newOffset = offset;
            }
        }
        if (newOffset == 0)return;
        if ((size = DB_dataTypeSizeGet(type)) == 0) size = stringLen(value);
        packOffset -= size;
        *valueOffset = packOffset;
        memmove(((void _FAR *) ((char _FAR *)groupResp + packOffset)), value, size);
        oldOffset = newOffset;
    }
#undef ATTRIB_TYPE
#undef KEY_TYPE
}


/* This function mallocs a string that contains the SL path and given filename */
char *SL_buildPathname(char *filename)
{
char *pathname;
unsigned short Length = 14;

    if(SL_path != (char *)NULL) Length += strlen(SL_path); /* Space for trailing '\ and '\0' */
	pathname = (char *)OS_Alloc(Length);

	if (pathname == NULL) return NULL;

    if((SL_path != (char *)NULL) && strlen(SL_path)){
    	strcpy(pathname, SL_path);
    	strcat(pathname, "\\");
    	return strcat(pathname, filename);
    }
	return strcpy(pathname, filename);
}
	

/* This function returns the length of a DMI string (length, data).          */
unsigned short stringLen(DMI_STRING _FAR *s)
{
    if(s == (DMI_STRING _FAR *)NULL) return 0;
	return (unsigned short) (alignmem(s->length) + sizeof(s->length));
}


/* This function copies the DMI string                                       */
DMI_STRING _FAR *stringCpy(DMI_STRING _FAR *dest, DMI_STRING _FAR *src)
{
	return (DMI_STRING _FAR *)memcpy(dest, src, stringLen(src));
}



/* This function compares two DMI strings                                    */
short stringCmp(DMI_STRING _FAR *str1, DMI_STRING _FAR *str2)
{
	if (str1->length != str2->length)
		return (1);
	else
		return (short) (memcmp(str1, str2, str1->length));
}




/* This functions appends one DMI string to another                          */
DMI_STRING _FAR *stringCat(DMI_STRING _FAR *dest, DMI_STRING _FAR *src)
{
	memmove(&dest->body[dest->length], src->body, (unsigned short)src->length);
	dest->length += src->length;
	return dest;
}

