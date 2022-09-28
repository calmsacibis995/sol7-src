// Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)regci.cc	1.9 96/10/07 Sun Microsystems"

#include <stdlib.h>
#include "dmi.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "mutex.hh"
#include "subscription.hh"
#include "trace.hh"
#include "regci.hh"

DmiAccessDataList_t *newAccessList(DmiAccessDataList_t* acc)
{
	if (acc == NULL) {
		return (NULL);
	}
	
	DmiAccessDataList_t *result =
		(DmiAccessDataList_t *) malloc (sizeof(DmiAccessDataList_t));
	if (result == NULL) {
		error("malloc error in newAccessList\n"); 
		return (NULL);
	}
	result->list.list_len = acc->list.list_len;
	if (result->list.list_len == 0) {
		result->list.list_val = NULL;
	}
	else {
		result->list.list_val = (DmiAccessData_t *)
			malloc(sizeof(DmiAccessData_t) * result->list.list_len);
		if ( result->list.list_val == NULL) {
			trace("malloc error in newAccessList \n"); 
			free (result);
			return (NULL);
		}
		for (int i = 0; i< result->list.list_len; i++) {
			result->list.list_val[i].groupId =
				acc->list.list_val[i].groupId;
			result->list.list_val[i].attributeId =
				acc->list.list_val[i].attributeId;
		}
	}
	return (result); 
}

void freeAccessList(DmiAccessDataList_t* acc)
{
	if (acc == NULL) return ;
	if (acc->list.list_val != NULL){
		free(acc->list.list_val);
		acc->list.list_val = NULL;
	}
	free(acc);
	acc = NULL; 
}

extern Component *getComponent(DmiId_t compId);

bool_t
dmiregisterci(DmiRegisterCiIN *argp, DmiRegisterCiOUT *result)
{
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->regInfo->compId);
	if (thisComp == NULL) {
		trace("comp %d not found\n",argp->regInfo->compId); 
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->handle = NULL;
		result->dmiSpecLevel = NULL;
		release_component_lock(); 
		return (FALSE); 
	}

	thisComp->SetProgNum(argp->regInfo->prognum);
	thisComp->SetAccessList(newAccessList(argp->regInfo->accessData));
	result->error_status = DMIERR_NO_ERROR;
	result->handle = &(argp->regInfo->compId);
	result->dmiSpecLevel = dmiinfo.dmiSpecLevel;
	release_component_lock(); 
	return (TRUE);
	
}


bool_t 
dmiunregisterci(DmiUnregisterCiIN *argp, DmiUnregisterCiOUT *result)
{
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->handle);
	if (thisComp == NULL) {
		trace("comp %d not register\n", argp->handle);
		result->error_status = DMIERR_UNKNOWN_CI_REGISTRY;
		release_component_lock(); 
		return (FALSE); 
	}

	thisComp->SetProgNum(0);
//	DmiAccessDataList_t *acc = thisComp->GetAccessList();
//	if (acc != NULL) {
//		freeAccessList( acc );
//		acc = NULL;
//		thisComp->SetAccessList(acc); 
//	}
	thisComp->SetAccessList(NULL); 
	result->error_status = DMIERR_NO_ERROR;
	release_component_lock(); 
	return (TRUE);
}

extern DmiNodeAddress_t *getSender();
extern void freeSender(DmiNodeAddress_t *sender); 
bool_t
dmioriginateevent(DmiOriginateEventIN *argp, DmiOriginateEventOUT *result)
{
	DmiDeliverEventIN deleveIn;
	
	deleveIn.handle = 0;
	deleveIn.sender = getSender();
	deleveIn.language = argp->language;
	deleveIn.compId = argp->compId;
	deleveIn.timestamp = argp->timestamp;
	printDmiTimestamp(deleveIn.timestamp ); 

	if (argp->rowData == NULL)
		deleveIn.rowData = NULL;
	else {
		DmiMultiRowData_t mrowdata;
		DmiRowData_t rowdata[1]; 
		mrowdata.list.list_len = 1;
		mrowdata.list.list_val = rowdata;
		rowdata[0].compId = argp->rowData->compId;
		rowdata[0].groupId = argp->rowData->groupId;
		rowdata[0].className = argp->rowData->className;
		rowdata[0].keyList = argp->rowData->keyList;
		rowdata[0].values = argp->rowData->values;
		deleveIn.rowData = &mrowdata;
	}
        mgt_dmideliverevent(&deleveIn); 

	freeSender(deleveIn.sender);
	trace("deliver event\n"); 
	return (TRUE);
}


