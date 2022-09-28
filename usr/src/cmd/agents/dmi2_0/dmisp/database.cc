// Copyright 11/01/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)database.cc	1.24 96/11/01 Sun Microsystems"

#include <stdlib.h>
#include "dmi.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "search_util.hh"
#include "mutex.hh"
#include "subscription.hh"
#include "dbapi.hh"
#include "trace.hh"
#include "database.hh"

extern Component *installnamedircomp();
DmiComponentInfo_t *newComponentInfo(DmiComponentInfo_t *compInfo)
{
	if (compInfo == NULL) return NULL;
	DmiComponentInfo_t *result = (DmiComponentInfo_t *) malloc
		(sizeof(DmiComponentInfo_t));

	if (result == NULL) {
		error("malloc error at newComponentInfo\n");
		return (NULL);
	}

	result->id = compInfo->id;
	result->name = newDmiStringFromDmiString(compInfo->name);
	result->pragma = newDmiStringFromDmiString(compInfo->pragma);
	result->description = newDmiStringFromDmiString(compInfo->description);
	result->exactMatch = compInfo->exactMatch;
	return (result);
}

void freeComponentInfo(DmiComponentInfo_t *compInfo)
{
	if (compInfo == NULL) return;
	freeDmiString(compInfo->name);
	freeDmiString(compInfo->pragma);
	freeDmiString(compInfo->description);
	free(compInfo);
	return; 
}

DmiGroupInfo_t *newGroupInfo(DmiGroupInfo_t *groupinfo)
{
	if (groupinfo == NULL) return NULL;
	DmiGroupInfo_t *result = (DmiGroupInfo_t *)malloc
		(sizeof(DmiGroupInfo_t));

	if (result == NULL) {
		error("malloc error at newGroupInfo\n");
		return (NULL);
	}
	
	result->id = groupinfo->id;
	result->name = newDmiStringFromDmiString(groupinfo->name);
	result->pragma = newDmiStringFromDmiString(groupinfo->pragma);
	result->className = newDmiStringFromDmiString(groupinfo->className);
	result->description = newDmiStringFromDmiString(groupinfo->description);
	result->keyList = newDmiAttributeIdsFromIds(groupinfo->keyList); 
	return (result);
}

void freeGroupInfo(DmiGroupInfo_t *groupinfo)
{
	if (groupinfo == NULL) return;

	
	freeDmiString(groupinfo->name);
	freeDmiString(groupinfo->pragma);
	freeDmiString(groupinfo->className);
	freeDmiString(groupinfo->description);
	freeDmiAttributeIds(groupinfo->keyList);
	free(groupinfo);
	return; 
}
	
#include <sys/utsname.h>
DmiNodeAddress_t *getSender()
{
	struct utsname thisuname; 
    
	if (uname( &thisuname) < 0) {
		return (NULL); 
	}

	DmiNodeAddress_t *result = (DmiNodeAddress_t *)
		malloc(sizeof(DmiNodeAddress_t));

	if (result == NULL) {
		error("malloc error at getSender\n");
		return (NULL);
	}
	result->address = newDmiString(thisuname.nodename);
	result->rpc = NULL;
	result->transport = NULL;
	return (result);
}

void freeSender(DmiNodeAddress_t *sender)
{
	if (sender == NULL) return;
	if (sender->address != NULL)
		freeDmiString(sender->address);
	if (sender->rpc != NULL)
		free(sender->rpc);
	if (sender->transport != NULL)
		free(sender->transport);
	free(sender);
	return; 
}

bool_t
dmiaddcomponent(DmiAddComponentIN *argp, DmiAddComponentOUT *result)
{
	if ((argp->fileData == NULL)|| (argp->fileData->list.list_len == 0)) {
		result->compId = 0;
		result->error_status = DMIERR_FILE_ERROR;
		result->errors = NULL;
		trace("no mif filename provided\n"); 
		return (FALSE);
	}

	for (int i= 0; i< 	argp->fileData->list.list_len; i++) {
		if ((argp->fileData->list.list_val[i].fileType != DMI_MIF_FILE_NAME) ||
			(argp->fileData->list.list_val[i].fileData->body.body_len > 64) ||
			(argp->fileData->list.list_val[i].fileData->body.body_val
			 == NULL)){
			result->compId = 0;
		result->error_status = DMIERR_FILE_ERROR;
		result->errors = NULL;
		trace("mif file type or filename is not correct\n"); 
		return (FALSE);
		}
	}

	char filename[65];
	int err; 
	for ( i= 0; i< 	argp->fileData->list.list_len; i++) {
		memcpy(filename,
			   argp->fileData->list.list_val[i].fileData->body.body_val,
			   argp->fileData->list.list_val[i].fileData->body.body_len);
		filename[argp->fileData->list.list_val[i].fileData->body.body_len]
			= '\0'; 
		acquire_component_lock();
		ComponentID++;
		Component *thisComp =
			CreateComponentFromMif(err, filename, ComponentID);

		if (thisComp == NULL) {
			switch (err) {
				case DMIERR_PARSING_ERROR:
					trace("parsing error in mif file %s.\n", filename);
					parseErr->printParseErr(); 
					break;
				case DMIERR_DB_NOT_INITIALIZE:
					trace("database is not initialized.\n");
					break;
				case DMIERR_DB_DIR_NOT_EXIST:
					trace("database directory does exist.\n");
					break;
				default:
					trace("unknown DB error.\n");
					break;
			}
			result->error_status = DMIERR_BAD_SCHEMA_DESCRIPTION_FILE;
			result->compId = 0;
			result->errors = NULL;
			ComponentID--;
			release_component_lock();
			return (FALSE); 
		}
		else {
			dmiinfo.components->append(thisComp);
			result->error_status = DMIERR_NO_ERROR;
			result->compId = thisComp->GetComponentId();
		}
		result->errors = NULL;

		DmiComponentAddedIN compaddedIn;
		compaddedIn.handle = 0;
		compaddedIn.sender = getSender();
		compaddedIn.info = newComponentInfo(thisComp->GetComponentInfo());
		WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
		if (err != DMIERR_NO_ERROR) {
			error("Database writing error, exit, check the permission.\n");
			release_component_lock();
			exit(1);
		}
		release_component_lock();

		mgt_dmicomponentadded(&compaddedIn);

		freeSender(compaddedIn.sender);
		freeComponentInfo(compaddedIn.info);
	}
	return (TRUE); 
}


bool_t dmiaddgroup(DmiAddGroupIN *argp, DmiAddGroupOUT *result)
{
	// should be unable to add 
	if (argp->compId == 1) {
		result->error_status = DMIERR_INSUFFICIENT_PRIVILEGES;
		result->groupId = 0;
		result->errors = NULL;
		trace("Cannot modify groups in Component %d\n", argp->compId ); 
		return (FALSE); 
	}

	if ((argp->fileData == NULL)|| (argp->fileData->list.list_len == 0)) {
		result->groupId = 0;
		result->error_status = DMIERR_FILE_ERROR;
		result->errors = NULL;
		trace("no group schema filename provided\n");
		return (FALSE);
	}
	
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->groupId = 0;
		result->errors = NULL;
		trace("component %d not exist\n", argp->compId); 
		release_component_lock(); 
		return (FALSE); 
	}
	for (int i= 0; i< 	argp->fileData->list.list_len; i++) {
		if ((argp->fileData->list.list_val[i].fileType != DMI_MIF_FILE_NAME) ||
			(argp->fileData->list.list_val[i].fileData->body.body_len > 64) ||
			(argp->fileData->list.list_val[i].fileData->body.body_val
			 == NULL)){
			result->groupId = 0;
			result->error_status = DMIERR_FILE_ERROR;
			result->errors = NULL;
			release_component_lock();
			trace("group schema file type or name is not correct\n"); 
			return (FALSE);
		}
	}

	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL ) {
		groups = new RWTIsvSlist<Group>();
		thisComp->SetGroups(groups); 
	}

	char filename[65];
	Group *thisGroup;
	int err; 
	for ( i= 0; i< 	argp->fileData->list.list_len; i++) {
		memcpy(filename,
			   argp->fileData->list.list_val[i].fileData->body.body_val,
			   argp->fileData->list.list_val[i].fileData->body.body_len);
		filename[argp->fileData->list.list_val[i].fileData->body.body_len]
			= '\0'; 
		DmiId_t thisGroupId = thisComp->GetUniqueGroupId(); 
		thisGroup = CreateGroup(err, filename, thisGroupId);
		if (thisGroup == NULL) {
			switch (err) {
				case DMIERR_PARSING_ERROR:
					trace("parsing error in group schema file %s.\n", filename);
					break;
				case DMIERR_DB_NOT_INITIALIZE:
					trace("database is not initialized.\n");
					break;
				case DMIERR_DB_DIR_NOT_EXIST:
					trace("database directory does exist.\n");
					break;
				default:
					trace("unknown DB error.\n");
					break;
			}
			result->error_status = DMIERR_BAD_SCHEMA_DESCRIPTION_FILE;
			result->groupId = 0;
			result->errors = NULL;
			release_component_lock();
			return (FALSE); 
		}
		insertGroup(groups, thisGroup);
	}
		
	result->error_status = DMIERR_NO_ERROR;
	result->groupId = thisGroup->GetGroupId();
	result->errors = NULL;
	WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
	if (err != DMIERR_NO_ERROR) {
		error("Database writing error, exit, check the permission.\n");
		release_component_lock();
		exit(1);
	}
		
	DmiGroupAddedIN groupaddedIn;
	groupaddedIn.handle = 0;
	groupaddedIn.sender = getSender();
	groupaddedIn.compId= argp->compId ;
	groupaddedIn.info = newGroupInfo(thisGroup->GetGroupInfo());
	release_component_lock();

	mgt_dmigroupadded(&groupaddedIn);

	freeSender(groupaddedIn.sender);
	freeGroupInfo(groupaddedIn.info); 
	return (TRUE);
}

bool_t
dmideletecomponent(DmiDeleteComponentIN *argp, DmiDeleteComponentOUT *result)
{
	int err;
	
	// should be unable to remove 
	if (argp->compId == 1) {
		trace("Component 1 can not be removed\n"); 
//		result->error_status = DMIERR_CANT_UNINSTALL_SP_COMPONENT;
		result->error_status = DMIERR_INSUFFICIENT_PRIVILEGES;
		return (FALSE); 
	}
	
	acquire_component_lock(); 
	
	if (theComponents == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("component %d not exist\n", argp->compId ); 
		release_component_lock(); 
		return (FALSE);
	}
	

	DeleteComponentFormDB(err, argp->compId);
	if (err != DMIERR_NO_ERROR) {
		error("Database updating error, exit, check the permission.\n");
		release_component_lock();
		exit(1);
	}


	Component *thisComp =
		theComponents->remove(*IsThisComponent, (void *) argp->compId);
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("component %d not exist\n", argp->compId );
		release_component_lock(); 
		return (FALSE);
	}

	delete thisComp; 

	
	result->error_status = DMIERR_NO_ERROR;

	DmiComponentDeletedIN compdeledIn;
	compdeledIn.handle = 0;
	compdeledIn.sender = getSender();
	compdeledIn.compId = argp->compId;
	release_component_lock();

	mgt_dmicomponentdeleted(&compdeledIn);
	
	freeSender(compdeledIn.sender); 

	return (TRUE); 
}


bool_t dmideletegroup(DmiDeleteGroupIN *argp, DmiDeleteGroupOUT *result)
{
	int err; 
	if (argp->compId == 1) {
		result->error_status = DMIERR_INSUFFICIENT_PRIVILEGES;
		trace("group in component 1 cannot be deleted\n"); 		
		return (FALSE);
	}
	
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("component %d not exist\n", argp->compId);
		release_component_lock(); 
		return (FALSE);
	}

	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL) {
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		trace("no group in component %d\n", argp->compId);
		release_component_lock(); 
		return (FALSE);
	}

	Group *thisGroup = groups->remove(*IsThisGroup, (void *)argp->groupId);
	if (thisGroup == NULL) {
		trace("group %d in component %d not found\n", argp->groupId, argp->compId);
		result->error_status = DMIERR_GROUP_NOT_FOUND;
		release_component_lock(); 
		return (FALSE);
	}
	else {

		WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
		if (err != DMIERR_NO_ERROR) {
			error("Database writing error, exit, check the permission.\n");
			release_component_lock();
			exit(1);
		}
		
		result->error_status = DMIERR_NO_ERROR;

		DmiGroupDeletedIN groupdeletedIn;
		groupdeletedIn.handle = 0;
		groupdeletedIn.sender = getSender();
		groupdeletedIn.compId = argp->compId;
		groupdeletedIn.groupId = argp->groupId;
		release_component_lock();

		mgt_dmigroupdeleted(&groupdeletedIn);

		freeSender(groupdeletedIn.sender); 
		return (TRUE);
	}
}

bool_t
dmiaddlanguage(DmiAddLanguageIN *argp, DmiAddLanguageOUT *result)
{
	result->error_status = DMIERR_INSUFFICIENT_PRIVILEGES;
	result->errors = NULL;
	trace("dmiaddlanguage is not supported. \n");
	return (FALSE);

#if 0
	acquire_component_lock(); 
	if ((argp->fileData == NULL)|| (argp->fileData->list.list_len == 0)) {
		result->error_status = DMIERR_FILE_ERROR;
		result->errors = NULL;
		trace("no language schema filename provided\n");
		return (FALSE);
	}

	for (int i= 0; i< 	argp->fileData->list.list_len; i++) {
		if ((argp->fileData->list.list_val[i].fileType != DMI_MIF_FILE_NAME) ||
			(argp->fileData->list.list_val[i].fileData->body.body_len > 64) ||
			(argp->fileData->list.list_val[i].fileData->body.body_val
			 == NULL)){
			result->error_status = DMIERR_FILE_ERROR;
			result->errors = NULL;
			release_component_lock();
			trace("language schema file type is not correct, or file name is too long.\n"); 
			return (FALSE);
		}
	}

	Component *thisComp = getComponent(argp->compId);
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		result->errors = NULL;
		trace("component %d not exist\n", argp->compId);
		release_component_lock(); 
		return (FALSE);
	}

	RWTPtrSlist<DmiString> *langs = thisComp->GetLanguages();
	if (langs == NULL) {
		langs = new RWTPtrSlist<DmiString>();
		thisComp->SetLanguages(langs);
	}

	char filename[65];
	int err;
	DmiString *dstr; 
	for ( i= 0; i< 	argp->fileData->list.list_len; i++) {
		memcpy(filename,
			   argp->fileData->list.list_val[i].fileData->body.body_val,
			   argp->fileData->list.list_val[i].fileData->body.body_len);
		filename[argp->fileData->list.list_val[i].fileData->body.body_len]
			= '\0';
		dstr = GetLanguageFromMif(err, filename);
		if (dstr == NULL) {
			switch (err) {
				case DMIERR_PARSING_ERROR:
					trace("parsing error in language schema file %s.\n", filename);
					break;
				case DMIERR_DB_NOT_INITIALIZE:
					trace("database is not initialized.\n");
					break;
				case DMIERR_DB_DIR_NOT_EXIST:
					trace("database directory does exist.\n");
					break;
				default:
					trace("unknown DB error.\n");
					break;
			}
			result->error_status = err;
			result->errors = NULL;
			release_component_lock();
			return (FALSE); 
		}
		langs->append(dstr);
	}
		

	WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
	if (err != DMIERR_NO_ERROR) {
		error("Database writing error, exit, check the permission.\n");
		release_component_lock();
		exit(1);
	}
	
	result->error_status = DMIERR_NO_ERROR;
	result->errors = NULL;
	

	DmiLanguageAddedIN langaddedIn; 
	langaddedIn.handle = 0;
	langaddedIn.sender = getSender();
	langaddedIn.compId = argp->compId;
	langaddedIn.language = newDmiStringFromDmiString(dstr);
	release_component_lock();


	mgt_dmilanguageadded(&langaddedIn);

	freeSender(langaddedIn.sender);
	freeDmiString(langaddedIn.language); 
	return (TRUE);
#endif	
}

RWBoolean IsThisLanguage(DmiString *dstr, void *d)
{
	if (!cmpDmiString(dstr, (DmiString *)d))
		return (TRUE);
	else
		return (FALSE); 
}

bool_t
dmideletelanguage(DmiDeleteLanguageIN *argp, DmiDeleteLanguageOUT *result)
{
	result->error_status = DMIERR_INSUFFICIENT_PRIVILEGES;
	trace("dmideletelanguage is not supported. \n");
	return (FALSE);

#if 0	
	int err; 
	acquire_component_lock(); 
	Component *thisComp = getComponent(argp->compId);	
	if (thisComp == NULL) {
		result->error_status = DMIERR_COMPONENT_NOT_FOUND;
		trace("component %d not exist\n", argp->compId);		
		release_component_lock(); 
		return (FALSE);
	}

	RWTPtrSlist<DmiString> *langs = thisComp->GetLanguages();
	if ((langs == NULL)||(langs->entries()==1)) {
		result->error_status = DMIERR_CANT_UNINSTALL_COMPONENT_LANGUAGE;
		trace("cannot delete language in component %d\n", argp->compId);		
		release_component_lock(); 
		return (FALSE);
	}
	DmiString *dstr = langs->remove(*IsThisLanguage, (void *)argp->language);
	if (dstr == NULL) {
		result->error_status = DMIERR_CANT_UNINSTALL_COMPONENT_LANGUAGE;
		trace("language not exist in component %d\n", argp->compId);				
		release_component_lock(); 
		return (FALSE);
	}
	
	WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
	if (err != DMIERR_NO_ERROR) {
		error("Database writing error, exit, check the permission.\n");
		release_component_lock();
		exit(1);
	}
	
	
	result->error_status = DMIERR_NO_ERROR;
	release_component_lock();

	DmiLanguageDeletedIN langdeledIn; 
	langdeledIn.handle = 0;
	langdeledIn.sender = getSender();
	langdeledIn.compId = argp->compId;
	langdeledIn.language = dstr;
	release_component_lock();
	
   	mgt_dmilanguagedeleted(&langdeledIn);
	freeSender(langdeledIn.sender); 
	freeDmiString(langdeledIn.language);
	return (TRUE);
#endif	
}
