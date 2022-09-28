// Copyright 10/16/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)operation.cc	1.33 96/10/16 Sun Microsystems"

#include <stdlib.h>
#include "dmi.hh"
#include "ci_callback.h"
#include "search_util.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "spcallci.hh"
#include "mutex.hh"

#include "dbapi.hh"
#include "trace.hh"
#include "operation.hh"
#include <synch.h>
#include <string.h>

extern cond_t subtbl_mod;

void cleanDmiDataUnion(DmiDataUnion_t *d1)
{
	if (d1 == NULL) return; 
	switch (d1->type) {
		case MIF_DATE:
			free(d1->DmiDataUnion_u.date);
			d1->DmiDataUnion_u.date = NULL; 
			break; 
		case MIF_OCTETSTRING:
			freeDmiOctetString(d1->DmiDataUnion_u.octetstring);
			d1->DmiDataUnion_u.octetstring = NULL; 
			break;
		case MIF_DISPLAYSTRING:
			free(d1->DmiDataUnion_u.str); 
			d1->DmiDataUnion_u.str = NULL; 
			break;
		default:
			break;
	}
	return; 
}

bool_t checkKeyList(DmiAttributeIds_t *keyIds, DmiAttributeValues_t *keyValues)
{
	if ((keyIds == NULL) || (keyValues == NULL))
		return TRUE;
	if (keyIds->list.list_len != keyValues->list.list_len)
		return FALSE;
	for (int i = 0; i< keyIds->list.list_len; i++) 
		if (keyIds->list.list_val[i] != keyValues->list.list_val[i].id)
			return FALSE;
	return (TRUE); 
}

bool_t checkReadPermission(Attribute *attr)
{
	if (attr == NULL) return FALSE;
	DmiAttributeInfo_t *attrinfo = attr->GetInfo();
	if ((attrinfo->access != MIF_READ_WRITE) &&
		(attrinfo->access != MIF_READ_ONLY))
		return (FALSE);
	else
		return (TRUE); 
}


bool_t checkWritePermission(Attribute *attr)
{
	if (attr == NULL) return FALSE;
	DmiAttributeInfo_t *attrinfo = attr->GetInfo();
	if ((attrinfo->access != MIF_READ_WRITE) &&
		(attrinfo->access != MIF_WRITE_ONLY))
		return (FALSE);
	else
		return (TRUE); 
}

bool_t dmigetattribute(DmiGetAttributeIN *argp, DmiGetAttributeOUT *result)
{
	acquire_component_lock();

	// check component
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL ) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->value = NULL;
		trace("component %d not found.\n", argp->compId); 
		release_component_lock(); 
		return (FALSE); 
	}

	Table *table = getTableOnId(thisComp, argp->groupId);
	if (table == NULL) { // for group
		Group *group = getGroup(thisComp, argp->groupId);
		if (group == NULL) {
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			result->value = NULL;
			trace("group %d of component %d not found.\n",
				  argp->groupId, argp->compId); 			
			release_component_lock(); 
			return (FALSE); 
		}

		Attribute *attr =
			getAttribute(group->GetAttributes(), argp->attribId);
		if (attr == NULL) {
			result->error_status = DMIERR_ATTRIBUTE_NOT_FOUND;
			result->value = NULL;
			trace("attr %d in group %d of component %d not found.\n",
				  argp->attribId, argp->groupId, argp->compId); 			
			release_component_lock(); 
			return (FALSE); 
		}

		if (checkReadPermission(attr) == FALSE) {
			result->error_status = DMIERR_ILLEGAL_TO_GET;
			result->value = NULL;
			trace("no read permission\n"); 
			release_component_lock(); 
			return (FALSE);
		}


		if (group->GetKeyList() != NULL ) {
			if (checkKeyList(group->GetKeyList(), argp->keyList) == FALSE) {
				result->error_status = DMIERR_ILLEGAL_KEYS;
				result->value = NULL;
				trace("no a valid keylist\n"); 
				release_component_lock(); 
				return (FALSE); 
			}
				
			if (thisComp->checkAccessList(argp->groupId, argp->attribId)
				== FALSE) {
				result->error_status = DMIERR_UNKNOWN_CI_REGISTRY;
				result->value = NULL;
				trace("group %d, attr %d not in access list\n",
					  argp->groupId, argp->attribId); 
				release_component_lock(); 
				return (FALSE); 
			}
			// get from component instrumentation code
			CiGetAttributeOUT cigetattOut;
			CiGetAttributeIN  cigetatt;

			cigetatt.componentId = argp->compId; 
			cigetatt.groupId = argp->groupId;
			cigetatt.attributeId = argp->attribId;
			cigetatt.language = NULL;
			cigetatt.keyList = argp->keyList;

			if (cigetattribute(thisComp->GetProgNum(), &cigetatt, &cigetattOut)
				== FALSE) {
				result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
				result->value = NULL;
				trace("unable to call component instrumentation program\n"); 
				release_component_lock(); 
				return (FALSE); 
			}
			if (cigetattOut.data != NULL)
				result->value = &((cigetattOut.data)->data) ;
			else
				result->value = NULL; 
//			result->error_status = DMIERR_NO_ERROR;
			result->error_status = cigetattOut.error_status;
			release_component_lock(); 
			return (TRUE);
		}
		else {
			// get default value
			result->value = attr->GetData();  
			result->error_status = DMIERR_NO_ERROR;
			release_component_lock(); 
			return (TRUE);
		}	
	}
	else { // for table
		// if it's table, we don't need to check the ci register, since we are
		// not going to ci part
		Group *group = getGroupOnClassName(thisComp, table->GetClassName());
		if (group == NULL) {
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			result->value = NULL;
			trace("template group for table %d not found.\n",
				  argp->groupId); 			
			release_component_lock();
			return (FALSE);
		}
			
		if (checkKeyList(group->GetKeyList(), argp->keyList) == FALSE) {
			result->error_status = DMIERR_ILLEGAL_KEYS;
			result->value = NULL;
			trace("no a valid keylist\n"); 			
			release_component_lock();
			return (FALSE);
		}
		
		DmiAttributeValues_t *row = findRow(table->GetRows(), argp->keyList);
		if (row == NULL) {
			result->error_status = 	DMIERR_ROW_NOT_FOUND;
			result->value = NULL;
			trace("row not exist\n");
			release_component_lock(); 
			return (FALSE);
		}
		
		DmiDataUnion_t *attrdataunion = getAttrData(row, argp->attribId);
		if (attrdataunion == NULL) {
			result->error_status = 	DMIERR_ATTRIBUTE_NOT_FOUND;
			result->value = NULL;
			trace("attr %d's data of group %d of comp %d is NULL.\n",
				  argp->attribId, argp->groupId, argp->compId);			
			release_component_lock(); 
			return (FALSE);
		}

		Attribute *attr = getAttribute(group->GetAttributes(), argp->attribId);
		if (checkReadPermission(attr) == FALSE) {
			result->error_status = DMIERR_ILLEGAL_TO_GET;
			result->value = NULL;
			trace("no read permission\n");
			release_component_lock(); 
			return (FALSE);
		}
		
		result->error_status = DMIERR_NO_ERROR;
		result->value = attrdataunion;
		release_component_lock(); 
		return (TRUE);
	}
}

DmiErrorStatus_t
dispatchGetRowRequest(DmiRowRequest *rowreq, DmiRowData_t *rowdata)
{
	if ((rowreq == NULL) || (rowdata == NULL)) {
		return (DMIERR_INSUFFICIENT_PRIVILEGES);
	}

	Component *thisComp = getComponent(rowreq->compId);
	if (thisComp == NULL ) {
		return (DMIERR_COMPONENT_NOT_FOUND); 
	}

	u_long prognum = thisComp->GetProgNum(); 

	DmiId_t attrId;
	RWTIsvSlist<Attribute> *attrs;
	Attribute *attr; 
	CiGetAttributeOUT cigetattOut;
	CiGetAttributeIN  cigetatt;

	CiGetNextAttributeOUT cigetnattOut;
	CiGetNextAttributeIN  cigetnatt;

	Table *table = getTableOnId(thisComp, rowreq->groupId);
	if (table == NULL) {
		Group *group =	getGroup(thisComp, rowreq->groupId);
		if (group == NULL) {
			return (DMIERR_GROUP_NOT_FOUND); 
		}

		attrs =	group->GetAttributes();
		if ((attrs == NULL) || (attrs->entries() == 0)) {
			return (DMIERR_ATTRIBUTE_NOT_FOUND);
		}

		DmiAttributeIds_t *groupKeyList = group->GetKeyList(); 
		DmiAttributeInfo_t *attrinfo;
		
		// if ids is omitted, get all the attrs
		if ((rowreq->ids == NULL)||(rowreq->ids->list.list_len == 0)) {
			// check permission
			for (int i = 0; i< attrs->entries(); i++) 
				if (checkReadPermission(attrs->at(i)) == FALSE) {
					trace("no read permission\n");
					return (DMIERR_ILLEGAL_TO_GET);
				}

			if (groupKeyList == NULL) {
				if (rowreq->requestMode != DMI_NEXT) {
					rowdata->values = newDmiAttributeValues(attrs->entries());
					for (int i = 0; i< attrs->entries(); i++) {
						attr = attrs->at(i);
						rowdata->values->list.list_val[i].id = attr->GetId(); 
						copyDmiDataUnion(
							&(rowdata->values->list.list_val[i].data),
							attr->GetData()) ;
					}
				}
				else {
					return (DMIERR_ROW_NOT_FOUND); 
				}
			}
			else {
				// check access list
				for (int i = 0; i< attrs->entries(); i++) {
					attr = attrs->at(i);
					if (thisComp->checkAccessList(rowreq->groupId,
												  attr->GetId())
						== FALSE) {
						trace("group %d, attr %d not in access list\n",
							  rowreq->groupId, attr->GetId()); 
						return  DMIERR_UNKNOWN_CI_REGISTRY;
					}
				}

				rowdata->values = newDmiAttributeValues(attrs->entries());
//				rowdata->values->list.list_len = attrs->entries(); 
					
 				for (i = 0; i< attrs->entries(); i++) {
					attrId = attrs->at(i)->GetId(); 
					switch (rowreq->requestMode) {
						case DMI_FIRST:
							cigetatt.componentId = rowreq->compId; 
							cigetatt.groupId = rowreq->groupId;
							cigetatt.attributeId = attrId;
							cigetatt.language = NULL;
							cigetatt.keyList = NULL; // get first row, 
							if (cigetattribute(prognum, &cigetatt, &cigetattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("error return in component code.\n"); 
								return (cigetattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetattOut.data) ;							
							break; 
						case DMI_UNIQUE:
							cigetatt.componentId = rowreq->compId; 
							cigetatt.groupId = rowreq->groupId;
							cigetatt.attributeId = attrId;
							cigetatt.language = NULL;
							cigetatt.keyList = rowreq->keyList;
							if (cigetattribute(prognum, &cigetatt, &cigetattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("error return in component code.\n"); 
								return (cigetattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetattOut.data) ;							
							break; 
						case DMI_NEXT:
							cigetnatt.componentId = rowreq->compId; 
							cigetnatt.groupId = rowreq->groupId;
							cigetnatt.attributeId = attrId;
							cigetnatt.language = NULL;
							cigetnatt.keyList = rowreq->keyList;
							if (cigetnextattribute(prognum, &cigetnatt, &cigetnattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetnattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("error return in component code.\n"); 
								return (cigetnattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetnattOut.data) ;							
							break;
						default:
							freeDmiAttributeValues(rowdata->values);
							rowdata->values = NULL;
							trace("unknown get mode\n"); 
							return (DMIERR_ILLEGAL_TO_GET);							
							break;
					}
				
				}
			}
		}
		// else get the attrs in the ids list
		else {
			for (int i = 0; i< rowreq->ids->list.list_len; i++) {
				attrId = rowreq->ids->list.list_val[i];
				attr = getAttribute(attrs, attrId);
				if (attr == NULL){
					trace("attr %d not found\n", attrId); 					
					return (DMIERR_ATTRIBUTE_NOT_FOUND);
				}
				if (checkReadPermission(attr) == FALSE) {
					trace("attr %d has no read permission\n", attrId); 
					return (DMIERR_ILLEGAL_TO_GET);
				}
			}
			if (groupKeyList == NULL) {
				if (rowreq->requestMode != DMI_NEXT) {
					rowdata->values = newDmiAttributeValues(rowreq->ids->list.list_len);
					for ( i = 0; i< rowreq->ids->list.list_len; i++) {
						attrId = rowreq->ids->list.list_val[i];
						attr = getAttribute(attrs, attrId);
						if (attr == NULL) {
							freeDmiAttributeValues(rowdata->values);
							rowdata->values = NULL;
							trace("attr %d not found\n", attrId); 
							return (DMIERR_ATTRIBUTE_NOT_FOUND);
						}
						rowdata->values->list.list_val[i].id = attrId; 
						copyDmiDataUnion(
							&(rowdata->values->list.list_val[i].data),
							attr->GetData()) ;
					}
				}
				else {
					return (DMIERR_ROW_NOT_FOUND);
				}
			}
			else {
				if (checkKeyList(groupKeyList, rowreq->keyList)== FALSE) {
					trace("not a valid keylist\n"); 
					return  DMIERR_ILLEGAL_KEYS;
				}
				for ( i = 0; i< rowreq->ids->list.list_len; i++) {
					attrId = rowreq->ids->list.list_val[i];
					if (thisComp->checkAccessList(rowreq->groupId,
												  attrId) == FALSE) {
						trace("group %d, attr %d not register\n",
							  rowreq->groupId, attrId); 
						return  DMIERR_UNKNOWN_CI_REGISTRY;
					}
				}
				rowdata->values = newDmiAttributeValues(rowreq->ids->list.list_len);
				for ( i = 0; i< rowreq->ids->list.list_len; i++) {
					attrId = rowreq->ids->list.list_val[i];
					switch (rowreq->requestMode) {
						case DMI_FIRST:
							cigetatt.componentId = rowreq->compId; 
							cigetatt.groupId = rowreq->groupId;
							cigetatt.attributeId = attrId;
							cigetatt.language = NULL;
							cigetatt.keyList = NULL; // get first row, 
							if (cigetattribute(prognum, &cigetatt, &cigetattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL;
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL; 
								trace("error return in component code.\n"); 
								return (cigetattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetattOut.data) ;							
							break; 
						case DMI_UNIQUE:
							cigetatt.componentId = rowreq->compId; 
							cigetatt.groupId = rowreq->groupId;
							cigetatt.attributeId = attrId;
							cigetatt.language = NULL;
							cigetatt.keyList = rowreq->keyList;
							if (cigetattribute(prognum, &cigetatt, &cigetattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL; 
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL; 
								trace("error return in component code.\n"); 
								return (cigetattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetattOut.data) ;							
							break; 
						case DMI_NEXT:
							cigetnatt.componentId = rowreq->compId; 
							cigetnatt.groupId = rowreq->groupId;
							cigetnatt.attributeId = attrId;
							cigetnatt.language = NULL;
							cigetnatt.keyList = rowreq->keyList;
							if (cigetnextattribute(prognum, &cigetnatt, &cigetnattOut) == FALSE) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL; 
								trace("unable to call component instrumentation program\n"); 
								return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
							}
							if (cigetnattOut.error_status != DMIERR_NO_ERROR) {
								freeDmiAttributeValues(rowdata->values);
								rowdata->values = NULL; 
								trace("error return in component code.\n"); 
								return (cigetnattOut.error_status);
							}
							copyDmiAttributeData(
								&(rowdata->values->list.list_val[i]),
								cigetnattOut.data) ;							
							break;
						default:
							freeDmiAttributeValues(rowdata->values);
							rowdata->values = NULL;
							trace("unknown get mode\n"); 
							return (DMIERR_ILLEGAL_TO_GET);							
							break;
					}
				}
			}
		}
		rowdata->compId = rowreq->compId;
		rowdata->groupId = rowreq->groupId;
		rowdata->className = group->GetClassName();
		rowdata->keyList = rowreq->keyList; 
		return DMIERR_NO_ERROR; 
			
	}
	// deal with table
	else {
		Group *group = getGroupOnClassName(thisComp, table->GetClassName());
		if (group == NULL) {
			trace("template group for table %d not found.\n",
				  rowreq->groupId); 			
			return(DMIERR_GROUP_NOT_FOUND);
		}
			
		if (checkKeyList(group->GetKeyList(), rowreq->keyList) == FALSE) {
			trace("not a valid keylist\n"); 
			return (DMIERR_ILLEGAL_KEYS);
		}
		DmiAttributeValues_t *row;
		RWTPtrSlist<DmiAttributeValues_t> *rows = table->GetRows();
		if (rows == NULL) {
			trace("No rows in this table %d\n", rowreq->groupId); 
			return (DMIERR_ROW_NOT_FOUND);
		}			
		switch (rowreq->requestMode) {
			case DMI_FIRST:
				row = rows->first(); 
				break;
			case DMI_UNIQUE:
				row = findRow(rows, rowreq->keyList);				
				break;
			case DMI_NEXT:
				row = findNextRow(rows, rowreq->keyList);				
				break;
			default:
				rowdata->values = NULL;
				trace("unknown get mode\n"); 
				return (DMIERR_ILLEGAL_TO_GET);							
				break;				
		}
		if (row == NULL) {
			trace("No such row in table %d\n", rowreq->groupId); 
			return (DMIERR_ROW_NOT_FOUND);
		}

		attrs = group->GetAttributes();
		if (attrs == NULL) {
			trace("the attr in template group of table %d not exist\n", rowreq->groupId); 
			return (DMIERR_ATTRIBUTE_NOT_FOUND);
		}
		
		if ((rowreq->ids == NULL)||(rowreq->ids->list.list_len == 0)) {
			rowdata->values = newDmiAttributeValues(attrs->entries());
			for (int i = 0; i< attrs->entries(); i++) {
				attr = attrs->at(i); 
				if (checkReadPermission(attr) == FALSE) {
					freeDmiAttributeValues(rowdata->values);
					rowdata->values = NULL;
					trace("attr %d has no read permission\n", i); 
					return (DMIERR_ILLEGAL_TO_GET);
				}
				rowdata->values->list.list_val[i].id = attr->GetId();
				DmiDataUnion_t *attrdataunion = getAttrData(row, attr->GetId());
				if (attrdataunion == NULL) {
					freeDmiAttributeValues(rowdata->values);
					rowdata->values = NULL;
					trace("attr %d has no data\n", attrId); 
					return (DMIERR_ATTRIBUTE_NOT_FOUND);
				}
				copyDmiDataUnion(
					&(rowdata->values->list.list_val[i].data),
					attrdataunion) ;
//					attr->GetData()) ;				
			}
		}
		else {
			rowdata->values = newDmiAttributeValues(rowreq->ids->list.list_len);
			for (int i = 0; i< rowreq->ids->list.list_len; i++) {
				attrId = rowreq->ids->list.list_val[i];
				attr = getAttribute(group->GetAttributes(), attrId);
				if (checkReadPermission(attr) == FALSE) {
					freeDmiAttributeValues(rowdata->values);
					rowdata->values = NULL;
					trace("attr %d has no read permission\n", attrId); 
					return (DMIERR_ILLEGAL_TO_GET);
				}

				DmiDataUnion_t *attrdataunion = getAttrData(row, attrId);
				if (attrdataunion == NULL) {
					freeDmiAttributeValues(rowdata->values);
					rowdata->values = NULL;
					trace("attr %d has no data\n", attrId); 
					return (DMIERR_ATTRIBUTE_NOT_FOUND);
				}
				rowdata->values->list.list_val[i].id = attrId; 
				copyDmiDataUnion(
					&(rowdata->values->list.list_val[i].data), attrdataunion) ;
			}
		}
		rowdata->compId = rowreq->compId;
		rowdata->groupId = rowreq->groupId;
		rowdata->className = table->GetClassName();
		rowdata->keyList = rowreq->keyList; 
		return DMIERR_NO_ERROR; 
		
	}
	
}


DmiMultiRowData_t theMultiRowData;
bool_t first_getmulti = TRUE; 
bool_t dmigetmultiple(DmiGetMultipleIN *argp, DmiGetMultipleOUT *result)
{
	acquire_component_lock(); 
	if ((argp->request == NULL) || (argp->request->list.list_len == 0)) {
		result->error_status = DMIERR_VALUE_UNKNOWN;
		result->rowData = NULL;
		trace("request is empty\n"); 
		release_component_lock();
		return (FALSE);
	}

	int i;
	// free multirowdata;
	if (first_getmulti != TRUE) {
		for (i = 0; i< theMultiRowData.list.list_len; i++) {
			freeDmiAttributeValues(theMultiRowData.list.list_val[i].values);
		}

	if (theMultiRowData.list.list_val != NULL)
		free(theMultiRowData.list.list_val);
	}
	
	
	theMultiRowData.list.list_len = argp->request->list.list_len;
	theMultiRowData.list.list_val = (DmiRowData_t *)
		malloc (sizeof (DmiRowData_t) *theMultiRowData.list.list_len);
	if (theMultiRowData.list.list_val == NULL) {
		error("malloc error in dmigetmultiple\n");
		theMultiRowData.list.list_len = 0; 
		result->rowData = NULL;
		result->error_status = DMIERR_OUT_OF_MEMORY;
		release_component_lock(); 
		return (FALSE);
	}
	
	for (i = 0; i<argp->request->list.list_len; i++) {
		if ((result->error_status =
			 dispatchGetRowRequest(&(argp->request->list.list_val[i]),
								   &(theMultiRowData.list.list_val[i]))) !=
			DMIERR_NO_ERROR) {
			free(theMultiRowData.list.list_val);
			theMultiRowData.list.list_val = NULL; 
			theMultiRowData.list.list_len = 0; 
			result->rowData = NULL;
			release_component_lock(); 
			return (FALSE);
		}
	}
	result->rowData = &theMultiRowData;
	first_getmulti = FALSE; 
	release_component_lock(); 
	return (TRUE); 
}



bool_t dmisetattribute(DmiSetAttributeIN *argp, DmiSetAttributeOUT *result)
{
	int err;
	
	if (argp->value == NULL) {
		trace("null set value\n"); 
		result->error_status = DMIERR_VALUE_UNKNOWN;
		return (FALSE);
	}
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL ) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("component %d not found\n", argp->compId); 
		release_component_lock(); 
		return (FALSE); 
	}


	Table *table = getTableOnId(thisComp, argp->groupId);
   	if (table == NULL) { // for group
		Group *group = getGroup(thisComp, argp->groupId);

		if (group == NULL) {
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			trace("group %d of component %d not found\n", argp->groupId, argp->compId); 
			release_component_lock(); 
			return (FALSE); 
		}
	
		Attribute *attr =
			getAttribute(group->GetAttributes(), argp->attribId);
		if (attr == NULL) {
			result->error_status = DMIERR_ATTRIBUTE_NOT_FOUND;
			trace("attr %d in group %d of component %d not found\n",
				  argp->attribId, argp->groupId, argp->compId); 
			release_component_lock(); 
			return (FALSE); 
		}

		DmiAttributeData attrdata;
		if (checkWritePermission(attr) == FALSE) {
			trace("attr %d in group %d of component %d has not write permission\n",
				  argp->attribId, argp->groupId, argp->compId); 
			result->error_status = DMIERR_ILLEGAL_TO_SET;
			release_component_lock(); 
			return (FALSE);
		}

		if (group->GetKeyList() != NULL) {
			if (checkKeyList(group->GetKeyList(), argp->keyList) == FALSE) {
				trace("not a valid keylist\n"); 
				result->error_status = DMIERR_ILLEGAL_KEYS;
				release_component_lock(); 
				return (FALSE); 
			}

			if (thisComp->checkAccessList(argp->groupId, argp->attribId) == FALSE) {
				result->error_status = DMIERR_UNKNOWN_CI_REGISTRY;
				trace("group %d, attr %d not register\n",
					  argp->groupId, argp->attribId); 
				release_component_lock(); 
				return (FALSE); 
			}

			switch (argp->setMode) {
				case DMI_SET:
					CiSetAttributeIN cisetatt;
					DmiErrorStatus_t cisetattOut;
				
					cisetatt.componentId = argp->compId; 
					cisetatt.groupId = argp->groupId;
					cisetatt.attributeId = argp->attribId;
					cisetatt.keyList = argp->keyList;
					cisetatt.language = NULL; 
					attrdata.id =  argp->attribId;
					copyDmiDataUnion(&(attrdata.data), argp->value); 
					cisetatt.data = &attrdata; 
					if (cisetattribute(thisComp->GetProgNum(), &cisetatt, &cisetattOut)
						== FALSE) {
						result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
						trace("unable to call component instrumentation program\n");
					}
					else
						result->error_status = cisetattOut;
					cleanDmiDataUnion(&(attrdata.data)); 
					release_component_lock();
					return (TRUE);
				case DMI_RESERVE:
					CiReserveAttributeIN cireserveatt;
					DmiErrorStatus_t cireserveattOut;

					cireserveatt.componentId = argp->compId; 
					cireserveatt.groupId = argp->groupId;
					cireserveatt.attributeId = argp->attribId;
					cireserveatt.keyList = argp->keyList;
					attrdata.id =  argp->attribId;
					copyDmiDataUnion(&(attrdata.data), argp->value);
					cireserveatt.data = &attrdata; 
					if (cireserveattribute(thisComp->GetProgNum(),
									   &cireserveatt,
										   &cireserveattOut) == FALSE) {
						trace("unable to call component instrumentation program\n");
						result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
					}
					else
						result->error_status = cireserveattOut; 
					cleanDmiDataUnion(&(attrdata.data)); 
					release_component_lock();
					return (TRUE);
				case DMI_RELEASE:
					CiReleaseAttributeIN cireleaseveatt;
					DmiErrorStatus_t cireleaseattOut;
				
					cireleaseveatt.componentId = argp->compId; 
					cireleaseveatt.groupId = argp->groupId;
					cireleaseveatt.attributeId = argp->attribId;
					cireleaseveatt.keyList = argp->keyList;
					attrdata.id =  argp->attribId;
					copyDmiDataUnion(&(attrdata.data), argp->value);
					cireleaseveatt.data = &attrdata; 
					if (cireleaseattribute(thisComp->GetProgNum(),
									   &cireleaseveatt,
										   &cireleaseattOut) == FALSE){
						trace("unable to call component instrumentation program\n");
						result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
					}
					else
						result->error_status = cireleaseattOut; 
					cleanDmiDataUnion(&(attrdata.data)); 
					release_component_lock();
					return (TRUE);
				default:
					result->error_status = DMIERR_ILLEGAL_TO_SET;
					trace("unknown set mode\n"); 
					release_component_lock(); 
					return (FALSE); 
			}
		}
		else {
			// change the default value
			if (attr->GetInfo()->type != argp->value->type) {
				trace("attr type and set value type are not same, cannot set\n"); 
				result->error_status = DMIERR_ILLEGAL_TO_SET;
				release_component_lock();
				return (FALSE);
			}
			if (argp->setMode == DMI_SET) {
				attr->SetData(argp->value);
			}
			WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
			if (err != DMIERR_NO_ERROR) {
				error("Database writing error, exit, check the permission.\n");
				release_component_lock();
				exit(1);
			}

			result->error_status = DMIERR_NO_ERROR;
			release_component_lock();
			return (TRUE);
		}
	}
	else { // for table
		Group *group = getGroupOnClassName(thisComp, table->GetClassName());
		if (group == NULL) {
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			trace("template group for table %d not found.\n",
				  argp->groupId); 			
			release_component_lock();
			return (FALSE);
		}
			
		if (checkKeyList(group->GetKeyList(), argp->keyList) == FALSE) {
			result->error_status = DMIERR_ILLEGAL_KEYS;
			trace(" not a valid keylist\n");
			release_component_lock();
			return (FALSE);
		}
		
		DmiAttributeValues_t *row = findRow(table->GetRows(), argp->keyList);
		if (row == NULL) {
			result->error_status = 	DMIERR_ROW_NOT_FOUND;
			trace("no such row exists\n"); 
			release_component_lock(); 
			return (FALSE);
		}
		
		DmiDataUnion_t *attrdataunion = getAttrData(row, argp->attribId);
		if (attrdataunion == NULL) {
			result->error_status = 	DMIERR_ATTRIBUTE_NOT_FOUND;
			trace("attr %d data has no data\n", argp->attribId);
			release_component_lock(); 
			return (FALSE);
		}

		Attribute *attr = getAttribute(group->GetAttributes(), argp->attribId);
		if (checkWritePermission(attr) == FALSE) {
			trace("attr %d in group %d of component %d has no write permission\n",
				  argp->attribId, argp->groupId, argp->compId);
			result->error_status = DMIERR_ILLEGAL_TO_SET;
			release_component_lock(); 
			return (FALSE);
		}

		if (attrdataunion->type != argp->value->type) {
			result->error_status = DMIERR_ILLEGAL_TO_SET;
			trace("attr type and set value type not match, unable to set\n"); 
			release_component_lock();
			return (FALSE);
		}

		if (argp->setMode == DMI_SET) {
			cleanDmiDataUnion(attrdataunion);
			copyDmiDataUnion(attrdataunion, argp->value);		
			WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
						if (err != DMIERR_NO_ERROR) {
				error("Database writing error, exit, check the permission.\n");
				release_component_lock();
				exit(1);
			}

		}
		result->error_status = DMIERR_NO_ERROR;
		release_component_lock(); 
		return (TRUE);
	}
}

DmiErrorStatus_t
dispatchSetRowRequest(DmiSetMode_t setmode, DmiRowData_t *rowData)
{

	int err;
	
	if (rowData == NULL) 
		return (DMIERR_VALUE_UNKNOWN);
	
	Component *thisComp = getComponent(rowData->compId);
	if (thisComp == NULL ) {
		trace("component %d not found\n", rowData->compId);
		return (DMIERR_COMPONENT_NOT_FOUND); 
	}

	
	u_long prognum = thisComp->GetProgNum(); 

	Table *table = getTableOnId(thisComp, rowData->groupId);

	DmiId_t attrId; 
	DmiAttributeInfo_t *attrinfo;
	Attribute *attr;

	if (table == NULL) {
		Group *group =	getGroup(thisComp, rowData->groupId);
		if (group == NULL) {
			trace("group %d of comp %d not found\n",
				  rowData->groupId, rowData->compId);
			return (DMIERR_GROUP_NOT_FOUND); 
		}

		DmiAttributeIds_t *groupKeyList = group->GetKeyList(); 		

		RWTIsvSlist<Attribute> *attrs =	group->GetAttributes();
		if (attrs == NULL) {
			trace("no attrs in group %d of comp %d \n",
				  rowData->groupId, rowData->compId);
			return (DMIERR_ATTRIBUTE_NOT_FOUND);
		}

		CiSetAttributeIN cisetatt;
		DmiErrorStatus_t cisetattOut;

		DmiAttributeData_t attrdata;
		if ((rowData->values == NULL) || (rowData->values->list.list_len == 0)) {
			trace("no row data\n");
			return DMIERR_VALUE_UNKNOWN;
		}
		for (int i = 0; i< rowData->values->list.list_len; i++) {
			attrId = rowData->values->list.list_val[i].id; 
			attr = getAttribute(attrs, attrId);
			if (attr == NULL) {
				trace("attr %d in group %d of comp %d not found\n",
					  attrId, rowData->groupId, rowData->compId);
				return (DMIERR_ATTRIBUTE_NOT_FOUND);
			}
			if (checkWritePermission(attr) == FALSE) {
				trace("attr %d in group %d of comp %d has no write permission\n",
					  attrId, rowData->groupId, rowData->compId);
				return (DMIERR_ILLEGAL_TO_SET);
			}
		}
		switch (setmode) {
			case DMI_SET:
				CiSetAttributeIN cisetatt;
				DmiErrorStatus_t cisetattOut;
				if (groupKeyList == NULL) {
					// get from local
					//scalable group
					for ( i = 0; i< rowData->values->list.list_len; i++) {
						attrId = rowData->values->list.list_val[i].id;
						attr =	getAttribute(group->GetAttributes(), attrId);
						if (attr == NULL) {
							trace("attr %d in group %d of comp %d not found\n",
								  attrId, rowData->groupId, rowData->compId);
							return (DMIERR_ATTRIBUTE_NOT_FOUND);
						}

						if (attr->GetInfo()->type != rowData->values->list.list_val[i].data.type) {
							trace("attr type and set value type not match, cannot set\n"); 
							return (DMIERR_ILLEGAL_TO_SET);
						}
						attr->SetData(&(rowData->values->list.list_val[i].data));
					}
					WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
					return (err); 
//					return (DMIERR_NO_ERROR);
				}
				else {
					for ( i = 0; i< rowData->values->list.list_len; i++) {
						attrId = rowData->values->list.list_val[i].id;
						if (thisComp->checkAccessList
							(rowData->groupId, attrId) == FALSE) {
							trace("group %d %d attr %d not register\n",
								  rowData->groupId, attrId); 
							return  DMIERR_UNKNOWN_CI_REGISTRY;
						}
						cisetatt.componentId = rowData->compId; 
						cisetatt.groupId = rowData->groupId;
						cisetatt.attributeId = attrId;
						cisetatt.language = NULL;
						cisetatt.keyList = rowData->keyList;
						attrdata.id = attrId;
						copyDmiDataUnion(&(attrdata.data),
										 &(rowData->values->list.list_val[i].data)); 
						cisetatt.data = &attrdata;
						if (cisetattribute(prognum, &cisetatt, &cisetattOut) == FALSE) {
							trace("unable to call component instrumentation program\n");
							return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
						}
						if (cisetattOut != DMIERR_NO_ERROR) {
							trace("error return in component code.\n");
							return (cisetattOut);
						}
					}
					return (DMIERR_NO_ERROR);
				}
			case DMI_RESERVE:
				CiReserveAttributeIN cireserveatt;
				DmiErrorStatus_t cireserveattOut;

				if (groupKeyList == NULL) {
					// do nothing
					return (DMIERR_NO_ERROR);
				}
				else {
					for ( i = 0; i< rowData->values->list.list_len; i++) {
						if (thisComp->checkAccessList
							(rowData->groupId, attrId) == FALSE) {
							trace("group %d %d attr %d not register\n",
								  rowData->groupId, attrId); 
							return  DMIERR_UNKNOWN_CI_REGISTRY;
						}
						attrId = rowData->values->list.list_val[i].id;
						cireserveatt.componentId = rowData->compId; 
						cireserveatt.groupId = rowData->groupId;
						cireserveatt.attributeId = attrId;
						cireserveatt.keyList = rowData->keyList;
						attrdata.id = attrId; 
						copyDmiDataUnion(&(attrdata.data),
										 &(rowData->values->list.list_val[i].data)); 
						cireserveatt.data = &attrdata;
						if (cireserveattribute(prognum, &cireserveatt,
											   &cireserveattOut) == FALSE){
							trace("unable to call component instrumentation program\n");
							return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
						}
						if (cireserveattOut != DMIERR_NO_ERROR){
							trace("error return in component code.\n");
							return (cireserveattOut);
						}
					}
					return (DMIERR_NO_ERROR);
				}

			case DMI_RELEASE:
				CiReleaseAttributeIN cireleaseveatt;
				DmiErrorStatus_t cireleaseattOut;

				if (groupKeyList == NULL) {
					// do nothing
					return (DMIERR_NO_ERROR);
				}
				else {
					for ( i = 0; i< rowData->values->list.list_len; i++) {
						if (thisComp->checkAccessList
							(rowData->groupId, attrId) == FALSE) {
							trace("group %d %d attr %d not register\n",
								  rowData->groupId, attrId); 
							return  DMIERR_UNKNOWN_CI_REGISTRY;
						}
						attrId = rowData->values->list.list_val[i].id;
						cireleaseveatt.componentId = rowData->compId; 
						cireleaseveatt.groupId = rowData->groupId;
						cireleaseveatt.attributeId = attrId;
						cireleaseveatt.keyList = rowData->keyList;
						attrdata.id = attrId; 
						copyDmiDataUnion(&(attrdata.data),
										 &(rowData->values->list.list_val[i].data)); 
						cireleaseveatt.data = &attrdata;
						if (cireleaseattribute(prognum, &cireleaseveatt,
											   &cireleaseattOut) == FALSE){
							trace("unable to call component instrumentation program\n");
							return (DMIERR_DIRECT_INTERFACE_NOT_REGISTERED);
						}
						if (cireleaseattOut != DMIERR_NO_ERROR ) {
							trace("error return in component code.\n");
							return(cireleaseattOut);
						}
					}
					return (DMIERR_NO_ERROR);
				}
			default:
				trace("unknown set mode\n"); 
				return (DMIERR_ILLEGAL_TO_SET);
		}
	}
	else { //for table
		// set new data in table
		// find the row first
		Group *group = getGroupOnClassName(thisComp, table->GetClassName());
		if (group == NULL) {
			trace("template group for table %d not found.\n",
				  rowData->groupId); 			
			return(DMIERR_GROUP_NOT_FOUND);
		}
			
		if (checkKeyList(group->GetKeyList(), rowData->keyList) == FALSE) {
			trace("not a valid keylist\n"); 
			return (DMIERR_ILLEGAL_KEYS);
		}
		
		DmiAttributeValues_t *row = findRow(table->GetRows(), rowData->keyList);
		if (row == NULL) {
			trace("no such row match the keylist\n"); 
			return (DMIERR_ROW_NOT_FOUND);
		}
		
		for ( int i = 0; i< rowData->values->list.list_len; i++) {
			attrId = rowData->values->list.list_val[i].id;
			DmiDataUnion_t *attrdataunion = getAttrData(row, attrId);
			if (attrdataunion == NULL) {
				trace("attr %d not found null\n", attrId); 
				return (DMIERR_ATTRIBUTE_NOT_FOUND);
			}

			Attribute *attr = getAttribute(group->GetAttributes(), attrId);
			if (checkWritePermission(attr) == FALSE) {
				trace("attr %d has no write permission\n",  attrId); 
				return (DMIERR_ILLEGAL_TO_SET);
			}

			if (attrdataunion->type != rowData->values->list.list_val[i].data.type) {
				trace("attr type and set value type not match\n"); 
				return (DMIERR_ILLEGAL_TO_SET);
			}

			if (setmode == DMI_SET) {
				cleanDmiDataUnion(attrdataunion);
				copyDmiDataUnion(attrdataunion, &(rowData->values->list.list_val[i].data));		
			}
		}
		WriteComponentToDB(err,thisComp, DB_UPDATE_ALL);
		return (err);
		//		return DMIERR_NO_ERROR; 	
	}
}

bool_t dmisetmultiple(DmiSetMultipleIN *argp, DmiSetMultipleOUT *result)
{
	acquire_component_lock(); 
	if ((argp->rowData == NULL) || (argp->rowData->list.list_len == 0)) {
     	result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("empty row data\n"); 
		release_component_lock(); 
		return (FALSE);
	}

	for (int i = 0; i<argp->rowData->list.list_len; i++) {
		if ((result->error_status =
			 dispatchSetRowRequest(argp->setMode,
								   &(argp->rowData->list.list_val[i]))) !=
			DMIERR_NO_ERROR) {
			release_component_lock(); 
			return (FALSE);
		}
	}
	release_component_lock(); 
	return (TRUE); 
}

	
bool_t dmiaddrow(DmiAddRowIN *argp, DmiAddRowOUT *result)
{
	int err;
	
	acquire_component_lock(); 
	if (argp->rowData == NULL) {
		trace("empty row data\n");
		result->error_status = DMIERR_VALUE_UNKNOWN;
		release_component_lock(); 
		return (FALSE);
	}

	Component *thisComp = getComponent(argp->rowData->compId);
	if (thisComp == NULL ) {
		trace("comp %d not found\n", argp->rowData->compId);
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		release_component_lock(); 
		return (FALSE); 
	}

	Table *table = getTableOnId(thisComp, argp->rowData->groupId);
	if (table == NULL) {
		//it's  a group
		Group *group = getGroup(thisComp, argp->rowData->groupId);
		if (group == NULL) {
			trace("group %d of comp %d not found\n",
				  argp->rowData->groupId, argp->rowData->compId);
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			release_component_lock(); 
			return (FALSE); 
		}
		if (group->GetKeyList() != NULL ) {
			if (group->IsRowValid(argp->rowData->values) == FALSE) {
				trace("not valid rowdata values\n"); 
				result->error_status = DMIERR_UNABLE_TO_ADD_ROW;
				release_component_lock();
				return (FALSE);
			}	
			if (thisComp->GetProgNum() == 0) {
				trace("comp %d ci code not running\n", argp->rowData->compId);
				result->error_status = DMIERR_UNKNOWN_CI_REGISTRY;
				release_component_lock();
				return (FALSE);
			}				
			// dispatch to component code
			CiAddRowIN  ciaddrowIn;
			ciaddrowIn.rowData = argp->rowData; 
			if (ciaddrow(thisComp->GetProgNum(), &ciaddrowIn, &(result->error_status))
				== FALSE) {
				trace("cannot call comp %d ci code\n", argp->rowData->compId);
				result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
				release_component_lock(); 
				return (FALSE); 
			}
			release_component_lock(); 
			return (TRUE);
		}

		else {
			// scale group, no table
			result->error_status = DMIERR_UNABLE_TO_ADD_ROW;
			trace("cannot add row to a scale group\n"); 
			release_component_lock(); 
			return (FALSE);
		}
	}
	else {
		//table
		RWTPtrSlist<DmiAttributeValues_t> *rows = table->GetRows(); 
		DmiAttributeValues_t *row = findRow(rows, argp->rowData->keyList);
		if (row != NULL) {
			trace("the row of table %d of comp %d exists!\n",
				  argp->rowData->groupId, argp->rowData->compId ); 
			result->error_status = 	DMIERR_ROW_EXIST;
			release_component_lock(); 
			return (FALSE);
		}

		if (rows == NULL) {
			rows = new RWTPtrSlist<DmiAttributeValues_t>();
			table->SetRows(rows);
		}
			
		// get refered group
		Group *group = getGroupOnClassName(thisComp, table->GetClassName());
		if (group == NULL) {
			trace("reference group of table %d not exist\n", argp->rowData->groupId); 
			result->error_status = 	DMIERR_GROUP_NOT_FOUND;
			release_component_lock(); 
			return (FALSE);
		}

		if (group->IsRowValid(argp->rowData->values) == FALSE) {
			trace("row values not valid, cannot added to the table\n"); 
			result->error_status = 	DMIERR_UNABLE_TO_ADD_ROW;
			release_component_lock(); 
			return (FALSE);
		}

		rows->append(newDmiAttributeValuesFromValues(argp->rowData->values));
		WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
			if (err != DMIERR_NO_ERROR) {
				error("Database writing error, exit, check the permission.\n");
				release_component_lock();
				exit(1);
			}
		result->error_status = 	DMIERR_NO_ERROR;
		DmiString_t *classname = table->GetClassName();
		if (strncmp(classname->body.body_val,
					"DMTF|SP Indication Subscription",
					strlen("DMTF|SP Indication Subscription")) == 0) {
			cond_signal(&subtbl_mod);
		}			
		release_component_lock(); 
		return (TRUE);
	}
}



bool_t dmideleterow(DmiDeleteRowIN *argp, DmiDeleteRowOUT *result)
{
	int err;
	
	acquire_component_lock(); 
	if (argp->rowData == NULL) {
		trace("empty row data\n"); 
		result->error_status = DMIERR_VALUE_UNKNOWN;
		release_component_lock(); 
		return (FALSE);
	}

	Component *thisComp = getComponent(argp->rowData->compId);
	if (thisComp == NULL ) {
		trace("comp %d not found\n", argp->rowData->compId);		
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		release_component_lock(); 
		return (FALSE); 
	}

	Table *table = getTableOnId(thisComp, argp->rowData->groupId);
	if (table == NULL) {
		//it's  a group
		Group *group = getGroup(thisComp, argp->rowData->groupId);
		if (group == NULL) {
			trace("group %d of comp %d not found\n",
				  argp->rowData->groupId, argp->rowData->compId);
			result->error_status = DMIERR_GROUP_NOT_FOUND;
			release_component_lock(); 
			return (FALSE); 
		}
		if (group->GetKeyList() != NULL ) {
			if (group->IsRowValid(argp->rowData->values) == FALSE) {
				trace("not valid rowdata values\n");
				result->error_status = DMIERR_UNABLE_TO_DELETE_ROW;
				release_component_lock();
				return (FALSE);
			}	
			if (thisComp->GetProgNum() == 0) {
				trace("comp %d ci code not running\n", argp->rowData->compId);
				result->error_status = DMIERR_UNKNOWN_CI_REGISTRY;
				release_component_lock();
				return (FALSE);
			}				
			// dispatch to component code
			CiDeleteRowIN cidelrowIn;
			cidelrowIn.rowData = argp->rowData; 
			if (cideleterow(thisComp->GetProgNum(),
							&cidelrowIn,
							&(result->error_status)) == FALSE) {
				trace("cannot call comp %d ci code\n", argp->rowData->compId);
				result->error_status = DMIERR_DIRECT_INTERFACE_NOT_REGISTERED;
				release_component_lock(); 
				return (FALSE); 
			}
			release_component_lock(); 
			return (TRUE);
		}
		else {
			// scale group, no table
			trace("cannot del row to a scale group\n");
			result->error_status = DMIERR_UNABLE_TO_DELETE_ROW;
			release_component_lock(); 
			return (FALSE);
		}
	}
	else {
		// table
		RWTPtrSlist<DmiAttributeValues_t> *rows = table->GetRows();
		if (rows == NULL) {
			trace("no row to delete\n"); 			
			result->error_status = 	DMIERR_ROW_NOT_FOUND;
			release_component_lock(); 
			return (FALSE);
		}
		DmiAttributeValues_t *row =
			rows->remove(*IsThisRowOnKey, (void *)argp->rowData->keyList); 
		if (row == NULL) {
			trace("no row to delete\n"); 
			result->error_status = 	DMIERR_ROW_NOT_FOUND;
			release_component_lock(); 
			return (FALSE);
		}
		WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);		
			if (err != DMIERR_NO_ERROR) {
				error("Database writing error, exit, check the permission.\n");
				release_component_lock();
				exit(1);
			}
		result->error_status = 	DMIERR_NO_ERROR;
		release_component_lock(); 
		return (TRUE);
	}
}

