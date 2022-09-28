#ifndef _SEARCH_UTIL_HH
#define _SEARCH_UTIL_HH
/* Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)search_util.hh	1.12 96/09/11 Sun Microsystems"

extern RWBoolean IsThisComponent(Component *comp, void *d); 
extern RWBoolean AfterThisComponent(Component *comp, void *d); 

extern RWBoolean IsThisGroup(Group *group, void *d); 
extern RWBoolean AfterThisGroup(Group *group, void *d); 

extern RWBoolean AfterThisTable(Table *table, void *d); 

extern RWBoolean IsThisAttr(Attribute *attr, void *d); 
extern RWBoolean AfterThisAttr(Attribute *attr, void *d); 

extern RWBoolean IsThisTableOnClassName(Table *tab, void *d); 
extern RWBoolean IsThisTableOnId(Table *tab, void *d); 

extern Component *getComponent(DmiId_t compId); 

extern Group *getGroup(Component *, DmiId_t groupId); 
extern Group *getGroupOnClassName(Component *, DmiString_t *);

extern Table *getTableOnClassName(Component *, DmiString_t *); 
extern Table *getTableOnId(Component *, DmiId_t); 

extern Attribute *
getAttribute(RWTIsvSlist<Attribute> *attrs, DmiId_t attrId); 
	
extern DmiDataUnion_t *
getAttrData(DmiAttributeValues_t *values, DmiId_t id); 

extern bool_t
IsKeyMatch(DmiAttributeValues_t *value, DmiAttributeValues_t *keylist); 

extern RWBoolean IsThisRowOnKey(DmiAttributeValues_t *row1, void *keylist); 
extern DmiAttributeValues_t *findRow(RWTPtrSlist<DmiAttributeValues_t> *values,
									 DmiAttributeValues_t *keylist); 
extern DmiAttributeValues_t *findNextRow(RWTPtrSlist<DmiAttributeValues_t> *values,
									 DmiAttributeValues_t *keylist); 

extern bool_t insertComp(RWTIsvSlist<Component> *comps, Component* comp); 
extern bool_t insertGroup(RWTIsvSlist<Group> *groups, Group* group); 
extern bool_t insertAttribute(RWTIsvSlist<Attribute> *attrs, Attribute* attr); 
#endif
