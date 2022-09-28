// Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)listing.cc	1.21 96/10/07 Sun Microsystems"


#include <stdlib.h>
#include "dmi.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "search_util.hh"
#include "mutex.hh"
#include "trace.hh"
#include "listing.hh"

int copyTableInfotoGroupInfo(DmiGroupInfo_t *dg, TableInfo *sg)
{
	if ((dg == NULL) || (sg== NULL)) return (-1); 
	dg->id = sg->id;
	dg->name = sg->name;
//	dg->pragma = NULL; 
	dg->className= sg->className;
//	dg->description = NULL; 
//	dg->keyList = NULL; 

	return (0); 
}

int
copyClassNameFromTable(DmiClassNameInfo_t *classinfo,
					   TableInfo *tableinfo)
{
	if ((classinfo == NULL) || (tableinfo == NULL)) return (-1);
	classinfo->id = tableinfo->id;
	classinfo->className = tableinfo->className;
	return (0); 
}


DmiComponentList_t thisCompList;
DmiComponentInfo_t *thisCompInfo = NULL;
bool_t
dmilistcomponents(DmiListComponentsIN *argp, DmiListComponentsOUT *result)
{
	acquire_component_lock(); 
	if ((theComponents == NULL)|| (theComponents->entries() == 0)) {
		thisCompList.list.list_len = 0;
		thisCompList.list.list_val = NULL;
		trace("no comps exist\n"); 
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisCompList;
		release_component_lock();
		return (TRUE); 
	}

	DmiId_t compId; 
	switch (argp->requestMode) {
		case DMI_UNIQUE:
			compId = argp->compId;
			break; 
		case DMI_FIRST:
			compId = 0; 
			break; 
		case DMI_NEXT:
			compId = argp->compId +1; 
			break;
	}

	int leng =
		theComponents->occurrencesOf(*AfterThisComponent,  (void *) compId);
	if (leng == 0) {
		thisCompList.list.list_len = 0;
		thisCompList.list.list_val = NULL;
		trace("no comps to list\n"); 
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisCompList;
		release_component_lock(); 
		return (TRUE);
	}
	else if ((argp->maxCount != 0) && (argp->maxCount < leng)) {
		thisCompList.list.list_len = argp->maxCount;
	}
	else {
		thisCompList.list.list_len = leng;
	}
		

	size_t index =  theComponents->index(*AfterThisComponent, (void *)compId);
	if (index == RW_NPOS) {
		trace("no comps to list\n"); 
		thisCompList.list.list_val  = NULL;
		thisCompList.list.list_len = 0;
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisCompList;
		release_component_lock(); 
		return (TRUE);
	}

	if (thisCompInfo != NULL) {
		free(thisCompInfo);
	}
	
	thisCompInfo = (DmiComponentInfo_t *)
		malloc(sizeof(DmiComponentInfo_t)*(thisCompList.list.list_len));
	if (thisCompInfo == NULL) {
		error("malloc error in dmilistcomponents\n");
		result->error_status = DMIERR_OUT_OF_MEMORY;
		result->reply = NULL;
		release_component_lock(); 
		return (FALSE);
	}
	
	int i = 0;
	Component *thisComp; 
	while (i < thisCompList.list.list_len ) {
		thisComp = theComponents->at(index);
		copyCompInfo(&(thisCompInfo[i]), thisComp->GetComponentInfo());
		index ++; 
		i++; 
	}
	thisCompList.list.list_val = thisCompInfo;
	result->error_status = DMIERR_NO_ERROR;
	result->reply = &thisCompList; 
	release_component_lock(); 
	return (TRUE); 
}


DmiGroupList_t thisGroupList; 
DmiGroupInfo_t *thisGroupInfo; 
bool_t
dmilistgroups(DmiListGroupsIN *argp, DmiListGroupsOUT *result)
{

	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		thisGroupList.list.list_len = 0;
		thisGroupList.list.list_val = NULL;
		trace("comp %d not found\n", argp->compId);
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisGroupList;
		release_component_lock(); 
		return (FALSE); 
	}

	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	RWTPtrSlist<Table> *tables = thisComp->GetTables();
	if ((groups == NULL)&&(tables== NULL)) {
		thisGroupList.list.list_len = 0;
		thisGroupList.list.list_val = NULL;

		trace("no groups in comp %d\n", argp->compId);
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = &thisGroupList;
		release_component_lock(); 
		return (FALSE);
	}

	DmiId_t groupId; 
	switch (argp->requestMode) {
		case DMI_UNIQUE:
			groupId = argp->groupId;
			break; 
		case DMI_FIRST:
			groupId = 0; 
			break; 
		case DMI_NEXT:
			groupId = argp->groupId + 1; 
			break;
	}
	size_t group_index, table_index;
	
	if (groups != NULL) {
		group_index = groups->index(*AfterThisGroup, (void *)groupId);
	}
	if (tables != NULL) {
		table_index = tables->index(*AfterThisTable, (void *)groupId);
	}
	if ((group_index == RW_NPOS)&&(table_index == RW_NPOS)) {
		trace("no groups in comp %d\n",  argp->compId);
		thisGroupList.list.list_val  = NULL;
		thisGroupList.list.list_len = 0;
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = &thisGroupList;
		release_component_lock(); 
		return (TRUE);
	}

	int leng = 0;
	int table_leng = 0;
	if (groups!= NULL)
		leng = 
			groups->occurrencesOf(*AfterThisGroup,  (void *) groupId);
	if (tables != NULL) 
		table_leng =
			tables->occurrencesOf(*AfterThisTable,  (void *) groupId);
	leng = leng + table_leng; 
		
	if (leng == 0) {
		thisGroupList.list.list_len = 0;
		thisGroupList.list.list_val = NULL;
		switch (argp->requestMode) {
			case DMI_UNIQUE:
				trace("no groups start from group %d in comp %d\n", groupId, argp->compId);
				break; 
			case DMI_FIRST:
				trace("no groups in comp %d\n", argp->compId);				
				break; 
			case DMI_NEXT:
				trace("no groups start from group %d in comp %d\n", groupId, argp->compId);
				break;
		}
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = &thisGroupList;
		release_component_lock(); 
		return (TRUE);
	}
	else if ((argp->maxCount != 0) && (argp->maxCount < leng)) {
		thisGroupList.list.list_len = argp->maxCount;
	}
	else {
		thisGroupList.list.list_len = leng;
	}

	if ( thisGroupInfo != NULL) {
		free(thisGroupInfo);
	}
	
	
	thisGroupInfo = (DmiGroupInfo_t *)
		malloc(sizeof(DmiGroupInfo_t)*(thisGroupList.list.list_len));

	int i = 0;
	Group *thisGroup;
	if (groups!= NULL) {
//		while (i < thisGroupList.list.list_len - table_leng ) {
		while ((i < leng - table_leng )&&(i <thisGroupList.list.list_len))  {
			thisGroup = groups->at(group_index);
			copyGroupInfo(&(thisGroupInfo[i]), thisGroup->GetGroupInfo());
			group_index ++; 
			i++; 
		}
	}
	Table *thisTable; 
	if (tables!= NULL) {
//		while (i < thisGroupList.list.list_len ) {
		while ((i < leng )&& (i < thisGroupList.list.list_len)) {
			thisTable = tables->at(table_index);
			thisGroup = getGroupOnClassName(thisComp, thisTable->GetClassName());
			if (thisGroup != NULL)
				copyGroupInfo(&(thisGroupInfo[i]), thisGroup->GetGroupInfo());
			copyTableInfotoGroupInfo(&(thisGroupInfo[i]),
									 thisTable->GetTableInfo());
			table_index ++; 
			i++; 
		}
	}
	thisGroupList.list.list_val = thisGroupInfo;
	result->error_status = DMIERR_NO_ERROR;
	result->reply = &thisGroupList;
	release_component_lock(); 
	return (TRUE); 
}

DmiAttributeList_t thisAttrList;
DmiAttributeInfo_t *thisAttrInfo = NULL; 
bool_t
dmilistattributes(DmiListAttributesIN *argp, DmiListAttributesOUT *result)
{
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		thisAttrList.list.list_len = 0;
		thisAttrList.list.list_val = NULL;
		trace("comp %d not found\n", argp->compId);		
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisAttrList;
		release_component_lock(); 
		return (FALSE); 
	}

	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	RWTPtrSlist<Table> *tables = thisComp->GetTables();
	if ((groups == NULL) && (tables == NULL)) {
		thisAttrList.list.list_len = 0;
		thisAttrList.list.list_val = NULL;
		trace("no groups in comp %d\n", argp->compId);		
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = &thisAttrList;
		release_component_lock(); 
		return (FALSE);
	}

	Group *thisGroup = NULL;
	
	if (groups != NULL) {
		thisGroup = groups->find(*IsThisGroup, (void *) argp->groupId);
	}
	if (thisGroup == NULL) {
		if (tables != NULL) {
			Table *thisTable = tables->find(*IsThisTableOnId, (void *) argp->groupId);
			if (thisTable == NULL) {
				thisAttrList.list.list_len = 0;
				thisAttrList.list.list_val = NULL;
				trace("group %d of comp %d not found\n",argp->groupId, argp->compId);
				result->error_status = DMIERR_GROUP_NOT_FOUND;
				result->reply = &thisAttrList;
				release_component_lock(); 
				return (FALSE);
			}
			else {
				thisGroup = getGroupOnClassName(thisComp, thisTable->GetClassName());
			}
		}
	}

	if (thisGroup == NULL) {
		thisAttrList.list.list_len = 0;
		thisAttrList.list.list_val = NULL;
		trace("group %d of comp %d not found\n",argp->groupId, argp->compId);		
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = &thisAttrList;
		release_component_lock(); 
		return (FALSE);
	}
	
	RWTIsvSlist<Attribute> *attrs = thisGroup->GetAttributes(); 
	if (attrs == NULL) {
		thisAttrList.list.list_len = 0;
		thisAttrList.list.list_val = NULL;
		trace("no attrs in group %d of comp %d\n",argp->groupId, argp->compId);
		result->error_status = DMIERR_ATTRIBUTE_NOT_FOUND;
		result->reply = &thisAttrList;
		release_component_lock(); 
		return (TRUE);
	}

	DmiId_t id; 
	switch (argp->requestMode) {
		case DMI_UNIQUE:
			id = argp->attribId;
			break; 
		case DMI_FIRST:
			id = 0; 
			break; 
		case DMI_NEXT:
			id = argp->attribId + 1; 
			break;
		default:
			trace("unknown request mode, use default DMI_UNIQUE.\n");
			id = argp->attribId;
			break; 
	}

	int leng =
		attrs->occurrencesOf(*AfterThisAttr,  (void *) id);
	if (leng == 0) {
		switch (argp->requestMode) {
			case DMI_UNIQUE:
			case DMI_NEXT:
				trace("no attrs start from %d in group %d of comp %d\n",
					  id, argp->groupId, argp->compId); 
				break; 
			case DMI_FIRST:
				trace("no attrs in group %d of comp %d\n",
					  argp->groupId, argp->compId); 
				break; 
		}
		result->error_status = DMIERR_ATTRIBUTE_NOT_FOUND;
		result->reply = NULL; 
		release_component_lock(); 
		return (TRUE);
	}
	else if ((argp->maxCount != 0) && (argp->maxCount < leng)) {
		thisAttrList.list.list_len = argp->maxCount;
	}
	else {
		thisAttrList.list.list_len = leng;
	}
	
	size_t index =  attrs->index(*AfterThisAttr, (void *)id);
	if (index == RW_NPOS) {
		trace("attrs not found\n"); 
		thisAttrList.list.list_val  = NULL;
		thisAttrList.list.list_len = 0;
		result->error_status = DMIERR_ATTRIBUTE_NOT_FOUND;
		result->reply = &thisAttrList;
		release_component_lock(); 
		return (TRUE);
	}

	if (thisAttrInfo != NULL)
		free(thisAttrInfo);
	
	thisAttrInfo = (DmiAttributeInfo_t *)
		malloc(sizeof(DmiAttributeInfo_t)*(thisAttrList.list.list_len));

	int i = 0;
	Attribute *thisAttr;
	while (i < thisAttrList.list.list_len ) {
		thisAttr = attrs->at(index);
		copyAttrInfo(&(thisAttrInfo[i]), thisAttr->GetInfo());
		index ++; 
		i++; 
	}
	thisAttrList.list.list_val = thisAttrInfo;
	result->error_status = DMIERR_NO_ERROR;
	result->reply = &thisAttrList;
	release_component_lock(); 
	return (TRUE); 
}

DmiClassNameList_t theClassNames;
DmiClassNameInfo_t *theClassNameInfo; 
bool_t
dmilistclassnames(DmiListClassNamesIN *argp, DmiListClassNamesOUT *result)
{
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId); 
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("comp %d not found\n", argp->compId);
		result->reply = NULL;
		release_component_lock(); 
		return (FALSE); 
	}

	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	RWTPtrSlist<Table> *tables = thisComp->GetTables();
	if ((groups == NULL)&&(tables == NULL)) {
		trace("no groups in comp %d\n", argp->compId);
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = NULL;
		release_component_lock(); 
		return (FALSE);
	}
	
	size_t leng = 0;
	if (groups != NULL)
		leng = groups->entries();
	size_t table_leng = 0;
	if (tables != NULL)
		table_leng = tables->entries();
	leng = leng + table_leng;
	
	if (leng == 0 ) {
		trace("no groups in comp %d\n", argp->compId);		
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		result->reply = NULL; 
		release_component_lock(); 
		return (FALSE);
	}
	if  ((argp->maxCount != 0) && (argp->maxCount <leng )) {
		theClassNames.list.list_len = argp->maxCount;
	}
	else
		theClassNames.list.list_len = leng;
	

	if (theClassNameInfo != NULL)
		free (theClassNameInfo);
	
	theClassNameInfo =
		(DmiClassNameInfo_t *)malloc(sizeof(DmiClassNameInfo_t)*
									 (theClassNames.list.list_len));
	int i = 0;
	Group *theGroup;
	Table *theTable;
	size_t group_index = 0;
	size_t table_index = 0;
	
	if (groups!= NULL) {
//		while (i < thisGroupList.list.list_len - table_leng ) {
		while ((i < leng - table_leng )&&(i <theClassNames.list.list_len))  {
			theGroup = groups->at(group_index);
			copyClassNameFromGroup(&(theClassNameInfo[i]),
								   theGroup->GetGroupInfo());
			group_index ++; 
			i++; 
		}
	}
	if (tables!= NULL) {
//		while (i < thisGroupList.list.list_len ) {
		while ((i < leng )&& (i < theClassNames.list.list_len)) {
			theTable = tables->at(table_index);
			copyClassNameFromTable(&(theClassNameInfo[i]),
								   theTable->GetTableInfo());
			table_index ++; 
			i++; 
		}
	}
	
/*	if (groups != NULL) {
		while (i< theClassNames.list.list_len - table_leng) {
			theGroup = groups->at(i);
			copyClassNameFromGroup(&(theClassNameInfo[i]),
								   theGroup->GetGroupInfo());
			i++; 
		}
	}
	if (tables != NULL) {
		while (i< theClassNames.list.list_len) {
			theTable = tables->at(i - theClassNames.list.list_len + table_leng);
			copyClassNameFromTable(&(theClassNameInfo[i]),
								   theTable->GetTableInfo());
			i++; 
		}
	}
	*/	
	theClassNames.list.list_val = theClassNameInfo; 
	result->error_status = DMIERR_NO_ERROR;
	result->reply = &theClassNames;
	release_component_lock(); 
	return (TRUE); 
}

typedef struct class_keyList {
	DmiString_t *className;
	DmiAttributeValues_t *keyList;
	DmiId_t id; 
};

RWBoolean IsThisComponentByClass(Component *comp, void *d)
{
	int i; 
	class_keyList *class_keylist = (class_keyList *) d;
	if (class_keylist == NULL) return FALSE;
	if (comp->GetComponentId() < class_keylist->id) return FALSE; 
	if (class_keylist->className == NULL) return FALSE;
	RWTIsvSlist<Group> *groups = comp->GetGroups();
	if (groups != NULL) {
		Group *group;
		for ( i = 0; i< groups->entries(); i++) {
			group = groups->at(i);
			if (cmpDmiString(group->GetClassName(),
							 class_keylist->className)== 0)
				return TRUE;
		}
	}
	RWTPtrSlist<Table> *tables = comp->GetTables(); 
	if (tables == NULL) return (FALSE);
	Table *table;
	for ( i = 0; i< tables->entries(); i++) {
		table = tables->at(i);
		if ((cmpDmiString(table->GetClassName(),
						  class_keylist->className)== 0)&&
			(findRow(table->GetRows(), class_keylist->keyList) != NULL))
			return TRUE;
	}
	return (FALSE);
}

bool_t dmilistcomponentsbyclass(DmiListComponentsByClassIN *argp,
								DmiListComponentsByClassOUT *result)
{
	acquire_component_lock(); 

	if ((theComponents == NULL)|| (theComponents->entries() == 0)) {
		thisCompList.list.list_len = 0;
		thisCompList.list.list_val = NULL;
		trace("no comps\n"); 
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisCompList;
		release_component_lock(); 
		return (FALSE); 
	}

	DmiId_t compId;
	switch (argp->requestMode) {
		case DMI_UNIQUE:
			compId = argp->compId;
			break; 
		case DMI_FIRST:
			compId = 0; 
			break; 
		case DMI_NEXT:
			compId = argp->compId +1; 
			break;
	}

	class_keyList class_keylist;
	class_keylist.id = compId;
	class_keylist.className = argp->className;
	class_keylist.keyList = argp->keyList;
	
	int leng =
		theComponents->occurrencesOf(*IsThisComponentByClass,
									 (void *)&(class_keylist));
	if (leng == 0) {
		thisCompList.list.list_len = 0;
		thisCompList.list.list_val = NULL;
		trace("comp not found\n"); 
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->reply = &thisCompList;
		release_component_lock(); 
		return (TRUE);
	}
	else if ((argp->maxCount != 0) && (argp->maxCount < leng)) {
		thisCompList.list.list_len = argp->maxCount;
	}
	else {
		thisCompList.list.list_len = leng;
	}

	if (thisCompInfo != NULL) {
		free(thisCompInfo);
	}
	
	thisCompInfo = (DmiComponentInfo_t *)
		malloc(sizeof(DmiComponentInfo_t)*(thisCompList.list.list_len));

	int i = 0;
	Component *thisComp; 
	while (i < thisCompList.list.list_len ) {
		thisComp = theComponents->find(*IsThisComponentByClass,
									   (void *)&(class_keylist));
		if (thisComp == NULL) {
			thisCompList.list.list_len = 0;
			thisCompList.list.list_val = NULL;
			trace("comp not found\n");		
			result->error_status = DMIERR_COMPONENT_NOT_FOUND;
			result->reply = &thisCompList;
			release_component_lock(); 
			return (TRUE);
		}			
		copyCompInfo(&(thisCompInfo[i]), thisComp->GetComponentInfo());
		class_keylist.id = thisComp->GetComponentId()+1;
		i++; 
	}
	thisCompList.list.list_val = thisCompInfo;
	result->error_status = DMIERR_NO_ERROR;
	result->reply = &thisCompList;
	release_component_lock(); 
	return (TRUE); 
}


DmiStringList_t thisLangList;
bool_t dmilistlanguages(DmiListLanguagesIN *argp,
						DmiListLanguagesOUT *result)
{
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		trace("comp not found\n", argp->compId);				
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		thisLangList.list.list_len = 0;
		thisLangList.list.list_val = NULL; 
		result->reply = &thisLangList;
		release_component_lock(); 
		return (FALSE);
	}
	
	RWTPtrSlist<DmiString> *langs = thisComp->GetLanguages();
	if (langs == NULL) {
		result->error_status = DMIERR_NO_ERROR;
		thisLangList.list.list_len = 0;
		thisLangList.list.list_val = NULL; 
		result->reply = &thisLangList;
		release_component_lock(); 
		return (TRUE);
	}

	thisLangList.list.list_len = langs->entries();
	if (thisLangList.list.list_len == 0) {
		result->error_status = DMIERR_NO_ERROR;
		thisLangList.list.list_len = 0;
		thisLangList.list.list_val = NULL; 
		result->reply = &thisLangList;
		release_component_lock(); 
		return (TRUE);
	}

	if (thisLangList.list.list_val != NULL)
		free(thisLangList.list.list_val);
	
	thisLangList.list.list_val = (DmiStringPtr_t *) malloc(
		sizeof(DmiStringPtr_t)*thisLangList.list.list_len);
	for (int i= 0; i< thisLangList.list.list_len; i++) {
		thisLangList.list.list_val[i] = langs->at(i);
//		printDmiString(thisLangList.list.list_val[i]); 
	}
	result->error_status = DMIERR_NO_ERROR;		
	result->reply = &thisLangList;
	release_component_lock(); 
	return (TRUE); 
}

