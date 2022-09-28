// Copyright 11/01/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dmi.cc	1.12 96/11/01 Sun Microsystems"

#include <stdlib.h>
#include <string.h>
#include "dmi.hh"
#include "util.hh"
#include "trace.hh"


Table::Table()
{
	tableInfo.id = 0;
	tableInfo.name = NULL; 
	tableInfo.className = NULL;
	timestamp = time(0); 
	rows = NULL; 
}

void Table::SetTableInfo(TableInfo *tblinfo)
{
	tableInfo.id = tblinfo->id;
	tableInfo.name = tblinfo->name;
	tableInfo.className = tblinfo->className;
	timestamp = time(0); 	
}

int Table::GetNumOfRows()
{
	if (rows == NULL)
		return 0;

	else
		return (rows->entries());
}

void Table::SetRows(RWTPtrSlist<DmiAttributeValues_t> *val)
{
	rows = val;
	timestamp = time(0); 		
}


Table::IsRowValid(DmiAttributeValues_t *row)
{
	if ((rows == NULL) || (rows->entries() == 0))  return FALSE; 
	if (row == NULL) return (FALSE);

	DmiAttributeValues_t *firstrow = rows->first(); 
	if (firstrow->list.list_len != row->list.list_len) return (FALSE);

	for (int i= 0; i<firstrow->list.list_len; i++) {
		if ((firstrow->list.list_val[i].id != row->list.list_val[i].id) ||
			(firstrow->list.list_val[i].data.type != row->list.list_val[i].data.type))
			return FALSE;
	}
	return (TRUE);

}

Attribute::Attribute()
{
	enum_flag = FALSE;
	enum_name = NULL; 
	attrInfo.id = 0;
	attrInfo.name = NULL;
	attrInfo.pragma = NULL;
	attrInfo.description = NULL;
	attrInfo.storage = MIF_COMMON;
	attrInfo.access = MIF_READ_ONLY; 
	attrInfo.type = MIF_DISPLAYSTRING;
	attrInfo.maxSize = 0;
	attrInfo.enumList = NULL;
	attrData.type = attrInfo.type;
	attrData.DmiDataUnion_u.str = NULL; 
}

bool_t Attribute::GetEnumFlag()
{
	return (enum_flag);
}

DmiString *Attribute::GetEnumName()
{
	return (enum_name);
}

void Attribute::SetEnumFlag(bool_t ef)
{
	enum_flag = ef;
}

void Attribute::SetEnumName(DmiString *dstr)
{
	enum_name = dstr;
}



bool_t Attribute::SetAttribute(DmiAttributeInfo_t attrinfo,
							   DmiDataUnion_t attrdata)
{
	if (attrinfo.type != attrdata.type) {
		printf("AttrInfo and AttrData are inconsist!\n");
		return (FALSE);
	}
	else {
		copyAttrInfo(&attrInfo, &attrinfo);
		copyDmiDataUnion(&attrData, &attrdata);
		return (TRUE); 
	}
}

void Attribute::SetData(DmiDataUnion_t *attrdata)
{
	attrData.type = attrdata->type; 
	switch (attrdata->type) {
		case MIF_COUNTER:
			attrData.DmiDataUnion_u.counter
				= attrdata->DmiDataUnion_u.counter;
			break; 
		case MIF_COUNTER64:
			attrData.DmiDataUnion_u.counter64[0]
				= attrdata->DmiDataUnion_u.counter64[0]; 
			attrData.DmiDataUnion_u.counter64[1]
				= attrdata->DmiDataUnion_u.counter64[1]; 
			break; 
		case MIF_GAUGE:
			attrData.DmiDataUnion_u.gauge
				= attrdata->DmiDataUnion_u.gauge; 
			break; 
		case MIF_INTEGER:
			attrData.DmiDataUnion_u.integer
				= attrdata->DmiDataUnion_u.integer; 
			break; 
		case MIF_INTEGER64:
			attrData.DmiDataUnion_u.integer64[0]
				= attrdata->DmiDataUnion_u.integer64[0]; 
			attrData.DmiDataUnion_u.integer64[1]
				= attrdata->DmiDataUnion_u.integer64[1]; 
			break; 
		case MIF_DATE:
			if (attrData.DmiDataUnion_u.date != NULL)
				free(attrData.DmiDataUnion_u.date); 
			attrData.DmiDataUnion_u.date
				= newDmiTimestampFromDmiTimestamp(attrdata->DmiDataUnion_u.date); 
			break; 
		case MIF_OCTETSTRING:
			if (attrData.DmiDataUnion_u.octetstring != NULL)
				freeDmiOctetString(attrData.DmiDataUnion_u.octetstring);
			attrData.DmiDataUnion_u.octetstring = newDmiOctetString
				(attrdata->DmiDataUnion_u.octetstring);
			break;
		case MIF_DISPLAYSTRING:
			if (attrData.DmiDataUnion_u.str != NULL)
				freeDmiString(attrData.DmiDataUnion_u.str);
			attrData.DmiDataUnion_u.str = newDmiStringFromDmiString
				(attrdata->DmiDataUnion_u.str);
			break;
		default:
			trace("unkown data type\n"); 
			break;
	}

}
	

Group::Group()
{
	groupInfo.id = 0;
	groupInfo.name = NULL; 
	groupInfo.pragma = NULL; 
	groupInfo.className = NULL; 
	groupInfo.description = NULL; 
	groupInfo.keyList = NULL;
//	timestamp = time(0); 
//	attributeInfos = NULL;
	attrs = NULL; 
}

/*
  Group::Group(DmiGroupInfo_t *groupinfo, RWTPtrSlist<DmiAttributeInfo_t> *attrinfos)
{
	groupInfo.id = groupinfo->id;
	groupInfo.name = groupinfo->name;
	groupInfo.pragma = groupinfo->pragma; 
	groupInfo.className = groupinfo->className; 
	groupInfo.description = groupinfo->description;
	groupInfo.keyList = groupinfo->keyList;
	attributeInfos = attrinfos; 	
}
*/

void Group::SetGroupInfo(DmiGroupInfo_t *groupinfo)
{
	groupInfo.id = groupinfo->id;
	groupInfo.name = groupinfo->name;
	groupInfo.pragma = groupinfo->pragma; 
	groupInfo.className = groupinfo->className; 
	groupInfo.description = groupinfo->description;
	groupInfo.keyList = groupinfo->keyList;
}

//void Group::SetAttributeInfos(RWTPtrSlist<DmiAttributeInfo_t> *attrinfos)
//{
//	attributeInfos = attrinfos; 	
//}

DmiGroupInfo_t *Group::GetGroupInfo()
{
	return &groupInfo;
}

Group::IsRowValid(DmiAttributeValues_t *row)
{
	if (attrs == NULL) return FALSE; 
	if (row == NULL) return (FALSE);

	if (attrs->entries() != row->list.list_len) return (FALSE);

	for (int i= 0; i< attrs->entries(); i++) {
		if ((attrs->at(i)->GetInfo()->id != row->list.list_val[i].id) ||
			(attrs->at(i)->GetInfo()->type != row->list.list_val[i].data.type))
			return FALSE;
	}
	return (TRUE);

}

//RWTPtrSlist<DmiAttributeInfo_t> *Group::GetAttributeInfos()
//{
//	return attributeInfos;
//}
	
#include "search_util.hh"

Component::Component()
{
	componentInfo.id = 0;
	componentInfo.name = NULL;
	componentInfo.pragma = NULL;
	componentInfo.description = NULL;
	componentInfo.exactMatch = 0;
	languages = NULL;
	groups = NULL;
	tables = NULL;
	prognum = 0;
	accessData = NULL;
	global_enumlist = NULL; 
}

Component::Component(DmiComponentInfo_t *compinfo,
					 RWTIsvSlist<Group> *thisgroups)
{
}


void Component::SetComponentInfo(DmiComponentInfo_t *compinfo)
{
	componentInfo.id = compinfo->id;
	componentInfo.name = compinfo->name ;
	componentInfo.pragma = compinfo->pragma;
	componentInfo.description = compinfo->description;
	componentInfo.exactMatch = compinfo->exactMatch;
}

void Component::SetAccessList(DmiAccessDataList_t *acc)
{
	if (accessData != NULL) {
		// free it first
		if (accessData->list.list_val != NULL) {
			free(accessData->list.list_val);
			accessData->list.list_val = NULL; 
		}
		free(accessData);
	}
	accessData = acc;
}


DmiComponentInfo_t *Component::GetComponentInfo()
{
	return &componentInfo; 
}

void Component::SetGroups(RWTIsvSlist<Group> *thisgroups)
{
	groups = thisgroups; 
}

RWTIsvSlist<Group> *Component::GetGroups()
{
	return groups; 
}


RWTPtrSlist<GlobalEnum> *Component::GetGlobalEnumList()
{
	return global_enumlist;
}

void Component::SetGlobalEnumList(RWTPtrSlist<GlobalEnum> *genumlist)
{
	global_enumlist = genumlist;
}


bool_t Component::checkAccessList(DmiId_t groupId, DmiId_t attribId)
{
	if (accessData == NULL) return (FALSE);
	if (accessData->list.list_len == 0) return (FALSE);
	for (int i= 0; i<accessData->list.list_len; i++) {
		if ((accessData->list.list_val[i].groupId == groupId) &&
			(accessData->list.list_val[i].attributeId == attribId)) {
			return (TRUE);
		}
	}
	return (FALSE); 
}

DmiId_t Component::GetUniqueGroupId()
{
	if ((groups == NULL) && (tables == NULL)) return (1);

	DmiId_t uniqueId = 1000;
	int found = 0; 

	if (groups != NULL) {
		while (!found ) {
			uniqueId++;
			if (groups->contains(*IsThisGroup, (void *) uniqueId) == FALSE) {
				found = 1;
			}
		}
	}
	else {
		uniqueId = 1;
	}

	if (tables != NULL) {
		while (1) {
			if (tables->contains(*IsThisTableOnId, (void *) uniqueId) == FALSE) {
				return (uniqueId); 
			}
			uniqueId++;
		}
	}
	else {
		return (uniqueId);
	}
}

ParseErr::ParseErr()
{
	parse_err[0] = '\0';
}

void ParseErr::setParseErr(char *err_str)
{
	if (err_str == NULL) {
		parse_err[0] = '\0';
		return ;
	}

	if (strlen(err_str) >= 526) {
		memcpy(parse_err, err_str, 511);
		parse_err[511] = '\0';
		return ;
	}

	strcpy(parse_err, err_str);
	return;

}

void ParseErr::printParseErr()
{
	trace("%s\n", parse_err); 
}
