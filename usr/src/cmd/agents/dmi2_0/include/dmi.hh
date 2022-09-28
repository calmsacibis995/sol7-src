#ifndef _DMI_HH
#define _DMI_HH
/* Copyright 10/30/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi.hh	1.24 96/10/30 Sun Microsystems"

#include "server.h"
#include <rw/tpslist.h>
#include <rw/tislist.h>
#include <rw/rstream.h>

struct TableInfo {
	DmiString_t *name; 
	DmiId_t id;
	DmiString_t *className;
};

typedef TableInfo TableInfo;

class Table: public RWIsvSlink {
	TableInfo tableInfo; 
	time_t timestamp; 
	RWTPtrSlist<DmiAttributeValues_t> *rows;
  public:
	Table();
	~Table() {}
 
	void SetTableInfo(TableInfo *tblinfo); 
	TableInfo *GetTableInfo() { return &tableInfo; }

	DmiString_t *GetClassName() { return tableInfo.className; }
	DmiId_t GetId() { return tableInfo.id; }

	void SetRows(RWTPtrSlist<DmiAttributeValues_t> *val); 
	RWTPtrSlist<DmiAttributeValues_t> *GetRows() { return rows; }
	int GetNumOfRows(); 
	time_t GetTimestamp() { return timestamp; }
	IsRowValid(DmiAttributeValues_t *row); 
};

class Attribute: public RWIsvSlink{
	DmiAttributeInfo_t attrInfo;
	DmiDataUnion_t attrData;
	bool_t enum_flag;
	DmiString_t *enum_name; 
  public:
	Attribute();
	~Attribute(){}
	bool_t SetAttribute(DmiAttributeInfo_t, DmiDataUnion_t);
	DmiId_t GetId() { return attrInfo.id; }
	DmiAttributeInfo_t *GetInfo() { return &attrInfo; }
	DmiDataUnion_t *GetData() { return &attrData; }
	void SetData(DmiDataUnion_t *attrdata);
	bool_t GetEnumFlag(); 
	DmiString *GetEnumName();
	void SetEnumFlag(bool_t ef); 
	void SetEnumName(DmiString *dstr); 
	
};

class Group: public RWIsvSlink{
	DmiGroupInfo_t groupInfo;
	RWTIsvSlist<Attribute> *attrs;
  public:
	Group(); 
	void SetGroupInfo(DmiGroupInfo_t *groupinfo);
	void SetAttributes(RWTIsvSlist<Attribute> *attributes)
	{ attrs = attributes; }
	DmiGroupInfo_t *GetGroupInfo();
	DmiString *GetClassName() { return (groupInfo.className); }
	RWTIsvSlist<Attribute> *GetAttributes() { return attrs; }
	DmiId_t GetGroupId() { return (groupInfo.id); }
	DmiAttributeIds_t *GetKeyList() { return (groupInfo.keyList); }

	IsRowValid(DmiAttributeValues_t *row); 
};

typedef struct GlobalEnum {
	DmiString *name;
	DmiEnumList_t *enumList;
}; 
	

class Component: public RWIsvSlink{
	DmiComponentInfo_t componentInfo;
	RWTPtrSlist<GlobalEnum> *global_enumlist; 
	RWTPtrSlist<DmiString> *languages; 
	RWTIsvSlist<Group> *groups;
	RWTPtrSlist<Table> *tables; 
	u_long prognum;
	/* an entry point of component callback functions.
	   decide which attributes and group to access */
	DmiAccessDataList_t *accessData;
	
  public:
	Component(); 
	Component(DmiComponentInfo_t *compinfo, RWTIsvSlist<Group> *groups); 
	void SetComponentInfo(DmiComponentInfo_t *compinfo); 
	DmiComponentInfo_t *GetComponentInfo();
	
	DmiId_t GetComponentId() { return (componentInfo.id); }
	DmiId_t GetUniqueGroupId(); 

	void SetGroups(RWTIsvSlist<Group> *groups); 
	RWTIsvSlist<Group> *GetGroups();

	RWTPtrSlist<DmiString> *GetLanguages() { return (languages); }
	void SetLanguages(RWTPtrSlist<DmiString> *langs)	{ languages = langs; }

	void SetTables(RWTPtrSlist<Table> *tabs) {tables = tabs; }
	RWTPtrSlist<Table> *GetTables() { return (tables); }

	void SetProgNum(ulong num) { prognum = num; }
	u_long GetProgNum() {return (prognum); }

	void SetAccessList(DmiAccessDataList_t *acc); 
	DmiAccessDataList_t *GetAccessList() {return (accessData); }

	bool_t checkAccessList(DmiId_t groupId, DmiId_t attribId);

	RWTPtrSlist<GlobalEnum> *GetGlobalEnumList(); 
	void SetGlobalEnumList(RWTPtrSlist<GlobalEnum> *genumlist); 

};


struct DmiInfo {
	RWTIsvSlist<Component> *components;
	DmiString_t *dmiSpecLevel;
	DmiString_t *description;
	DmiFileTypeList_t *fileTypes;
	DmiString_t *language;
};

typedef struct DmiInfo DmiInfo_t;

extern DmiInfo_t dmiinfo; 
#define theComponents dmiinfo.components

extern DmiId_t ComponentID; 


class ParseErr {
	char parse_err[512];
  public:
	ParseErr();
	~ParseErr(){}

	void setParseErr(char *err_str);
	void printParseErr(); 
};

extern ParseErr *parseErr; 

#endif

