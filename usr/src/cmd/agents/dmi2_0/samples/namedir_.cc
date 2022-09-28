// Copyright 09/25/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)namedir_callbacks.cc	1.5 96/09/25 Sun Microsystems"

#include <stdlib.h>
#include <string.h>
#include "ci_callback.h"
#include "dmi_error.hh"
#include "util.hh"
#include "trace.hh"

/* definition of user code */
typedef struct _Name{
    int    index;
    char    *name;
    char    *Last;
    char    *SL;
} Name;

Name *Names; 
int first, last, length; 
DmiString *developer = NULL, *company = NULL, *platform = NULL; 
DmiAttributeData_t data;

char developer1[] = "developer1";
char developer2[] = "developer2";
char developer3[] = "developer3";
char SunSoft[] = "SunSoft";
char OS_platform[] = "Solaris 2.6";
/* end of definition */

void CiInitialization()
{

	/*
	 * insert user' scode here
	 */
	Name default_Names[] = {10, developer1, SunSoft, OS_platform,
							20, developer2, SunSoft, OS_platform,
							30, developer3, SunSoft, OS_platform};

	first = 10;
	last = 30;
	length = 3; 
	Names = (Name *)malloc(sizeof(Name)*length);
	for (int i = 0; i< 3; i++) {
		Names[i].index = (i+1) *10;
		Names[i].name = default_Names[i].name;
		Names[i].Last = default_Names[i].Last;
		Names[i].SL = default_Names[i].SL;
	}
}

void CiGetAttribute(CiGetAttributeIN *argp, CiGetAttributeOUT *result)
{
	/*
	 * insert user's code here
	 */

	trace("CiGetAttribute called\n"); 
	if (argp->groupId != 2) {
		result->data = NULL;
		result->error_status = DMIERR_GROUP_NOT_FOUND;
	}

	int index = first; // default set to the first one
	if ((argp->keyList != NULL) && (argp->keyList->list.list_len != 0)) {
		// get key value, we just take the first key in the keyList
		if ((argp->keyList->list.list_val[0].data.type == MIF_INTEGER)&&
			(argp->keyList->list.list_val[0].id == 10)){
			index =
				argp->keyList->list.list_val[0].data.DmiDataUnion_u.integer; 
		}
        if((index < first) || (index > last)) {
			result->data = NULL;
			result->error_status = DMIERR_ILLEGAL_KEYS;
			return;
		}
    }

	switch (data.id = argp->attributeId){
        case 10:  /* index number */
            data.data.type = MIF_INTEGER;
			data.data.DmiDataUnion_u.integer = index; 
            break;
        case 20:  /* developers name */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(developer); 
			developer = newDmiString(Names[index/10 -1].name);
			data.data.DmiDataUnion_u.str = developer; 
            break;
        case 30:  /* Developers Employer */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(company); 
			company = newDmiString(Names[index/10 -1].Last);
			data.data.DmiDataUnion_u.str = company; 
            break;
        case 40:  /* platform */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(platform); 
			platform = newDmiString(Names[index/10 -1].SL);
			data.data.DmiDataUnion_u.str = platform; 
            break;
        default:    /* this is not an attribute ID we understand */
			result->data = NULL;
			result->error_status = DMIERR_VALUE_UNKNOWN;
			return; 
			break; 
    }
	result->data = &data;
	result->error_status = DMIERR_NO_ERROR;
	return; 
}

void
CiGetNextAttribute(CiGetNextAttributeIN *argp, CiGetNextAttributeOUT *result)
{
	/*
	 * insert user's code here
	 */
	
	trace("CiGetNextAttribute called\n"); 

	if (argp->groupId != 2) {
		result->data = NULL;
		result->error_status = DMIERR_GROUP_NOT_FOUND;
	}

	int index = first; // default set to the first one
	if ((argp->keyList != NULL) && (argp->keyList->list.list_len != 0)) {
		// get key value, we just take the first key in the keyList
		if ((argp->keyList->list.list_val[0].data.type == MIF_INTEGER)&&
			(argp->keyList->list.list_val[0].id == 10)){
			index =
				argp->keyList->list.list_val[0].data.DmiDataUnion_u.integer;
			index += 10; 
		}
        if((index < first) || (index > last)) {
			result->data = NULL;
			result->error_status = DMIERR_ROW_NOT_FOUND;
			return;
		}
    }

	switch (data.id = argp->attributeId){
        case 10:  /* index number */
            data.data.type = MIF_INTEGER;
			data.data.DmiDataUnion_u.integer = index; 
            break;
        case 20:  /* developers name */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(developer); 
			developer = newDmiString(Names[index/10 -1].name);
			data.data.DmiDataUnion_u.str = developer; 
            break;
        case 30:  /* Developers Employer */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(company); 
			company = newDmiString(Names[index/10 -1].Last);
			data.data.DmiDataUnion_u.str = company; 
            break;
        case 40:  /* platform */
			data.data.type = MIF_DISPLAYSTRING;
			freeDmiString(platform); 
			platform = newDmiString(Names[index/10 -1].SL);
			data.data.DmiDataUnion_u.str = platform; 
            break;
        default:    /* this is not an attribute ID we understand */
			result->data = NULL;
			result->error_status = DMIERR_VALUE_UNKNOWN;
			return; 
			break; 
    }
	result->data = &data;
	result->error_status = DMIERR_NO_ERROR;
	return; 
}

void
CiSetAttribute(CiSetAttributeIN *argp, DmiErrorStatus_t *result)
{
	/*
	 * insert user's code here
	 */
	
	trace("CiSetAttribute called\n");

	if (argp->groupId != 2) {		
		*result = DMIERR_GROUP_NOT_FOUND;
	}

	int index = first; // default set to the first one
	if ((argp->keyList != NULL) && (argp->keyList->list.list_len != 0)) {
		// get key value, we just take the first key in the keyList
		if ((argp->keyList->list.list_val[0].data.type == MIF_INTEGER)&&
			(argp->keyList->list.list_val[0].id == 10)){
			index = argp->keyList->list.list_val[0].data.DmiDataUnion_u.integer; 
		}
        if((index < first) || (index > last)) {
			*result = DMIERR_ILLEGAL_KEYS;
			return;
		}
    }

	if (argp->attributeId != argp->data->id) {
		trace("attribute id not match\n"); 
		*result = DMIERR_ILLEGAL_TO_SET;
	}
		
	switch (argp->attributeId){
        case 10:  /* index number */
			trace("attribute 10 is read only\n"); 
			*result = DMIERR_ILLEGAL_TO_SET;
            break;
        case 20:  /* developers name */
			if (argp->data->data.type != MIF_DISPLAYSTRING) {
				*result = DMIERR_ILLEGAL_TO_SET;
			}
			else {
				if (argp->data->data.DmiDataUnion_u.str->body.body_len != 0) {
//					free(Names[index/10 -1].name); 
					Names[index/10 -1].name = (char *) malloc
						(argp->data->data.DmiDataUnion_u.str->body.body_len +1);
					memcpy(Names[index/10 -1].name,
						   argp->data->data.DmiDataUnion_u.str->body.body_val,
						   argp->data->data.DmiDataUnion_u.str->body.body_len);
					Names[index/10 -1].name[argp->data->
										   data.DmiDataUnion_u.str->
										   body.body_len] = '\0';
				}
				else {
					Names[index/10 -1].name = NULL;
				}
			}
            break;
        case 30:  /* Developers Employer */
			if (argp->data->data.type != MIF_DISPLAYSTRING) {
				*result = DMIERR_ILLEGAL_TO_SET;
			}
			else {
				if (argp->data->data.DmiDataUnion_u.str->body.body_len != 0) {
					Names[index/10 -1].Last = (char *) malloc
						(argp->data->data.DmiDataUnion_u.str->body.body_len +1);
					memcpy(Names[index/10 -1].Last,
						   argp->data->data.DmiDataUnion_u.str->body.body_val,
						   argp->data->data.DmiDataUnion_u.str->body.body_len);
					Names[index/10 -1].Last[argp->data->
										   data.DmiDataUnion_u.str->
										   body.body_len] = '\0';
				}
				else {
					Names[index/10 -1].Last = NULL;
				}
			}
            break;
        case 40:  /*Platform*/
			if (argp->data->data.type != MIF_DISPLAYSTRING) {
				*result = DMIERR_ILLEGAL_TO_SET;
			}
			else {
				if (argp->data->data.DmiDataUnion_u.str->body.body_len != 0) {
					Names[index/10 -1].SL = (char *) malloc
						(argp->data->data.DmiDataUnion_u.str->body.body_len +1);
					memcpy(Names[index/10 -1].SL,
						   argp->data->data.DmiDataUnion_u.str->body.body_val,
						   argp->data->data.DmiDataUnion_u.str->body.body_len);
					Names[index/10 -1].SL[argp->data->
										 data.DmiDataUnion_u.str->
										 body.body_len] = '\0';
				}
				else {
					Names[index/10 -1].SL = NULL;
				}
			}
            break;
        default:    /* this is not an attribute ID we understand */
			*result = DMIERR_VALUE_UNKNOWN;
			return; 
    }
	
	*result = DMIERR_NO_ERROR;
	return; 
}

void
CiReserveAttribute(CiReserveAttributeIN *argp, DmiErrorStatus_t *result)
{
	/*
	 * insert user's code here
	 */
	
	trace("CiReserveAttribute called\n"); 
	if (argp->groupId != 2) {
		*result = DMIERR_GROUP_NOT_FOUND;
	}
	*result = DMIERR_NO_ERROR; 
}

void
CiReleaseAttribute(CiReleaseAttributeIN *argp, DmiErrorStatus_t *result)
{
	/*
	 * insert user's code here
	 */
	trace("CiReleaseAttribute called\n"); 
	if (argp->groupId != 2) {
		*result = DMIERR_GROUP_NOT_FOUND;
	}
	*result = DMIERR_NO_ERROR; 
}

void
CiAddRow(CiAddRowIN *argp, DmiErrorStatus_t *result)
{
	/*
	 * insert user's code here
	 */
	trace("CiAddRow called\n");
	*result = DMIERR_NO_ERROR; 
}

void
CiDeleteRow(CiDeleteRowIN *argp, DmiErrorStatus_t *result)
{
	/*
	 * insert user's code here
	 */
	trace("CiDeleteRow called\n");		
	*result = DMIERR_NO_ERROR; 	
}
