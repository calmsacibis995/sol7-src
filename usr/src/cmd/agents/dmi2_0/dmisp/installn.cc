// Copyright 08/01/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)installnamedir.cc	1.6 96/08/01 Sun Microsystems"

#include <stdlib.h>
#include "dmi.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "search_util.hh"
#include "mutex.hh"

bool_t
installCompIDGroup(Component *thisComp)
{
	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL ) {
		groups = new RWTIsvSlist<Group>();
		thisComp->SetGroups(groups); 
	}

	Group *thisGroup = new Group();
	DmiGroupInfo_t groupinfo;

	groupinfo.name = newDmiString("ComponentID");
	groupinfo.pragma = NULL;
	groupinfo.id = 1; 
	groupinfo.className = newDmiString("DMTF|ComponentID|1.0");
	groupinfo.description = newDmiString("This group defines attributes common to all components. This group is required.");
	groupinfo.keyList = NULL;
	thisGroup->SetGroupInfo(&groupinfo);

	RWTIsvSlist<Attribute> *attrs = thisGroup->GetAttributes();
	if (attrs == NULL) {
		attrs = new RWTIsvSlist<Attribute>();
		thisGroup->SetAttributes(attrs);
	}

// attributes:
	Attribute *thisAttr;
	DmiAttributeInfo_t attrinfo;
	DmiDataUnion_t attrdata;

	thisAttr = new Attribute();
	attrinfo.id = 1;
	attrinfo.name = newDmiString("Manufacturer");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The name of the manufacturer that produces this component.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("IBM Corp."); 
	attrinfo.maxSize = 64;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 2;
	attrinfo.name = newDmiString("Product");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The name of the component.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("DMTF Demonstration"); 
	attrinfo.maxSize = 64;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 
	
	thisAttr = new Attribute();
	attrinfo.id = 3;
	attrinfo.name = newDmiString("Version");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The version for the component.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("Version 1.0"); 
	attrinfo.maxSize = 64;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 
	
	thisAttr = new Attribute();
	attrinfo.id = 4;
	attrinfo.name = newDmiString("Serial Number");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The serial number for this instance of this component.");
	attrinfo.storage = MIF_SPECIFIC;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("1.00000");
	attrinfo.maxSize = 64;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 5;
	attrinfo.name = newDmiString("Installation");
	attrinfo.pragma = NULL;
	attrinfo.description =
		newDmiString("The time and date of the last install of this component.");
	attrinfo.storage = MIF_SPECIFIC;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("Solaris 2.5"); 
	attrinfo.maxSize = 32;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	insertGroup(groups,thisGroup); 
	return (TRUE);
}

bool_t
installNamesGroup(Component *thisComp)
{
	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL ) {
		groups = new RWTIsvSlist<Group>();
		thisComp->SetGroups(groups); 
	}

	Group *thisGroup = new Group();
	DmiGroupInfo_t groupinfo;

	groupinfo.name = newDmiString("Names");
	groupinfo.pragma = NULL;
	groupinfo.id = 2; 
	groupinfo.className = newDmiString("DMTF|DevNames|1.0");
	groupinfo.description = newDmiString("DMTF Developers names. Direct Interface Version");

	// set the key list
	groupinfo.keyList = newDmiAttributeIds(1);
	groupinfo.keyList->list.list_len = 1;
	groupinfo.keyList->list.list_val[0] = 10; 
	thisGroup->SetGroupInfo(&groupinfo);

	// attributes:
	RWTIsvSlist<Attribute> *attrs = thisGroup->GetAttributes();
	if (attrs == NULL) {
		attrs = new RWTIsvSlist<Attribute>();
		thisGroup->SetAttributes(attrs);
	}
	Attribute *thisAttr;
	DmiAttributeInfo_t attrinfo;
	DmiDataUnion_t attrdata;
	
	thisAttr = new Attribute();
	attrinfo.id = 10;
	attrinfo.name = newDmiString("Index");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("Index into names table.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_INTEGER;
	attrdata.DmiDataUnion_u.integer = 1; 
	attrinfo.maxSize = sizeof(DmiInteger_t);
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 20;
	attrinfo.name = newDmiString("Name");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("Name of the developer.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_WRITE;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("Name of dev."); 
	attrinfo.maxSize = 32;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 30;
	attrinfo.name = newDmiString("Company");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("Corporation that this developer works for.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_WRITE;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("SunSoft");
	attrinfo.maxSize = 32;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 40;
	attrinfo.name = newDmiString("Operating System / Env.");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The Operating system or Environment that this developer built code for.");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_WRITE;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString("Solaris 2.5"); 
	attrinfo.maxSize = 32; 
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	insertGroup(groups,thisGroup); 
	return (TRUE);
}

bool_t
installSoftwareTemp(Component *thisComp)
{
	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL ) {
		groups = new RWTIsvSlist<Group>();
		thisComp->SetGroups(groups); 
	}

	Group *thisGroup = new Group();
	DmiGroupInfo_t groupinfo;

	groupinfo.name = newDmiString("Software Template");
	groupinfo.pragma = newDmiString("SNMP:1.2.3.4.5.6"); 
	groupinfo.id = thisComp->GetUniqueGroupId();   // no id 
	groupinfo.className = newDmiString("DMTF|Software Example|001");
	groupinfo.description = NULL; 

	groupinfo.keyList = newDmiAttributeIds(1);
	groupinfo.keyList->list.list_len = 1;
	groupinfo.keyList->list.list_val[0] = 1; 
	thisGroup->SetGroupInfo(&groupinfo);

	// attributes:
	RWTIsvSlist<Attribute> *attrs = thisGroup->GetAttributes();
	if (attrs == NULL) {
		attrs = new RWTIsvSlist<Attribute>();
		thisGroup->SetAttributes(attrs);
	}
	Attribute *thisAttr;
	DmiAttributeInfo_t attrinfo;
	DmiDataUnion_t attrdata;
	
	thisAttr = new Attribute();
	attrinfo.id = 1;
	attrinfo.name = newDmiString("Product Name");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The name of the product");
	attrinfo.storage = MIF_COMMON;
	attrinfo.access = MIF_READ_ONLY;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = NULL; 
	attrinfo.maxSize = 64;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	thisAttr = new Attribute();
	attrinfo.id = 2;
	attrinfo.name = newDmiString("Product Version");
	attrinfo.pragma = NULL;
	attrinfo.description = newDmiString("The product's version number");
	attrinfo.storage = MIF_SPECIFIC;
	attrinfo.access = MIF_READ_WRITE;
	attrdata.type = attrinfo.type = MIF_DISPLAYSTRING;
	attrdata.DmiDataUnion_u.str = newDmiString(""); 
	attrinfo.maxSize = 32;
	attrinfo.enumList = NULL;
	thisAttr->SetAttribute(attrinfo, attrdata);
	attrs->append(thisAttr); 

	insertGroup(groups,thisGroup); 
	return (TRUE);
}
bool_t
installSoftwareTable(Component *thisComp)
{
	RWTPtrSlist<Table> *tables = thisComp->GetTables();
	if (tables == NULL ) {
		tables = new RWTPtrSlist<Table>();
		thisComp->SetTables(tables); 
	}

	Table *thisTable = new Table();
	TableInfo tableinfo;

	tableinfo.name = newDmiString("Software Template");
	tableinfo.id = 42;  
	tableinfo.className = newDmiString("DMTF|Software Example|001");
	thisTable->SetTableInfo(&tableinfo);

	// row values
	RWTPtrSlist<DmiAttributeValues_t> *rows = new RWTPtrSlist<DmiAttributeValues_t>();
	DmiAttributeValues_t *row = newDmiAttributeValues(2);
	row->list.list_len = 2;
	row->list.list_val[0].id = 1 ;
	row->list.list_val[0].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Circus");
	row->list.list_val[1].id = 2 ;
	row->list.list_val[1].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("4.0a");
	rows->append(row);
	
	row = newDmiAttributeValues(2);
	row->list.list_len = 2;
	row->list.list_val[0].id = 1 ;
	row->list.list_val[0].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Disk Blaster");
	row->list.list_val[1].id = 2 ;
	row->list.list_val[1].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("2.0c");
	rows->append(row);
	
	row = newDmiAttributeValues(2);
	row->list.list_len = 2;
	row->list.list_val[0].id = 1 ;
	row->list.list_val[0].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Oleo");
	row->list.list_val[1].id = 2 ;
	row->list.list_val[1].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("3.0");
	rows->append(row);

	row = newDmiAttributeValues(2);
	row->list.list_len = 2;
	row->list.list_val[0].id = 1 ;
	row->list.list_val[0].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Presenter");
	row->list.list_val[1].id = 2 ;
	row->list.list_val[1].data.type = MIF_DISPLAYSTRING;
	row->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("1.2");
	rows->append(row);

	thisTable->SetRows(rows); 

	tables->append(thisTable); 
	return (TRUE);
}

Component *installnamedircomp()
{
	Component *thisComponent = new Component();

	DmiComponentInfo_t compinfo;

	ComponentID++;
	compinfo.id = ComponentID; 


	compinfo.name = newDmiString("DMTF Developers -  Direct Interface Version"); 
	compinfo.pragma = NULL; 
	compinfo.description = newDmiString("This is a list of the people who actually wrote the code.");
	compinfo.exactMatch = 0;
	thisComponent->SetComponentInfo(&compinfo);

	installCompIDGroup(thisComponent);
	installNamesGroup(thisComponent);
	installSoftwareTemp(thisComponent);
	installSoftwareTable(thisComponent);
	
	if (dmiinfo.components == NULL) {
		dmiinfo.components = new RWTIsvSlist<Component>(); 
	}
	dmiinfo.components->append(thisComponent);

	return (thisComponent); 
}
