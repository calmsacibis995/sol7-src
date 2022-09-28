// Copyright 09/16/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)search_util.cc	1.12 96/09/16 Sun Microsystems"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dmi.hh"
#include "util.hh"

RWBoolean IsThisComponent(Component *comp, void *d)
{
	if (comp->GetComponentId() == (DmiId_t) d)
		return (TRUE);
	else return (FALSE);
}

RWBoolean AfterThisComponent(Component *comp, void *d)
{
	if (comp->GetComponentId() >= (DmiId_t) d)
		return (TRUE);
	else return (FALSE);
}

RWBoolean IsThisGroup(Group *group, void *d)
{
	if (group->GetGroupId() == (DmiId_t ) d) return TRUE;
	else return FALSE;
}

RWBoolean IsThisGroupOnClassName(Group *group, void *d)
{
	if (!cmpDmiString(group->GetClassName(), (DmiString *)d))
		return (TRUE);
	else return FALSE;
}

RWBoolean AfterThisGroup(Group *group, void *d)
{
	if (group->GetGroupId() >= (DmiId_t) d)
		return (TRUE);
	else
		return (FALSE);
}

RWBoolean AfterThisTable(Table *table, void *d)
{
	if (table->GetId() >= (DmiId_t) d)
		return (TRUE);
	else
		return (FALSE);
}


RWBoolean IsThisAttr(Attribute *attr, void *d)
{
	if (attr->GetId() == (DmiId_t) d)
		return (TRUE);
	else
		return (FALSE); 
}

RWBoolean AfterThisAttr(Attribute *attr, void *d)
{
	if (attr->GetId() >= (DmiId_t) d)
		return (TRUE);
	else
		return (FALSE);
}

RWBoolean IsThisTableOnClassName(Table *tab, void *d)
{
	if (!cmpDmiString(tab->GetClassName(), (DmiString *)d))
		return (TRUE);
	else
		return (FALSE); 
}

RWBoolean IsThisTableOnId(Table *tab, void *d)
{
	if (tab->GetId() == (DmiId_t) d)
		return (TRUE);
	else
		return (FALSE); 
}

Component *getComponent(DmiId_t compId)
{
	if ((theComponents == NULL)|| (theComponents->entries() == 0)) {
		return (NULL); 
	}
	return (theComponents->find(*IsThisComponent, (void *)compId)); 
}

Group *getGroup(Component *thisComp, DmiId_t groupId)
{
	if (thisComp == NULL) return (NULL);
	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL) return (NULL);
	return (groups->find(*IsThisGroup, (void *)groupId)); 
}

Group *getGroupOnClassName(Component *thisComp, DmiString_t *classname )
{
	if (thisComp == NULL) return (NULL);
	RWTIsvSlist<Group> *groups = thisComp->GetGroups();
	if (groups == NULL) return (NULL);
	return (groups->find(*IsThisGroupOnClassName, (void *)classname)); 
}

Table *getTableOnClassName(Component *thisComp, DmiString_t *classname)
{
	if (thisComp == NULL) return (NULL);
	RWTPtrSlist<Table> *tabs = thisComp->GetTables();
	if (tabs == NULL) return (NULL);
	return (tabs->find(IsThisTableOnClassName, (void *)classname));
}

Table *getTableOnId(Component *thisComp, DmiId_t id)
{
	if (thisComp == NULL) return (NULL);
	RWTPtrSlist<Table> *tabs = thisComp->GetTables();
	if (tabs == NULL) return (NULL);
	return (tabs->find(IsThisTableOnId, (void *) id));
}


Attribute *getAttribute(RWTIsvSlist<Attribute> *attrs, DmiId_t attrId)
{
	if (attrs == NULL) return (NULL);
	return (attrs->find(IsThisAttr, (void *) attrId));  
}
	
DmiDataUnion_t *
getAttrData(DmiAttributeValues_t *values, DmiId_t id)
{
	if (values == NULL) return (NULL);
	if (values->list.list_len == 0) return (NULL);
	for (int i = 0; i< values->list.list_len; i++) {
		if (values->list.list_val[i].id == id)
			return (&(values->list.list_val[i].data));
	}
	return (NULL);
}

/*extern bool_t cmpDmiAttributeValues(DmiAttributeValues_t *v1, DmiAttributeValues_t *v2);

RWBoolean IsThisRowOnRow(DmiAttributeValues_t *row1, void *row)
{
	DmiAttributeValues_t *row2 = (DmiAttributeValues_t *)row; 
	
	if (cmpDmiAttributeValues(row1, row2) == TRUE)
		return (TRUE);
	else return FALSE; 
}
*/

// The keylist len can be less than attribute length
RWBoolean IsThisRowOnKey(DmiAttributeValues_t *row, void *keyList)
{
	if (row == NULL) return (FALSE);
	DmiAttributeValues_t *keylist = (DmiAttributeValues_t *)keyList; 		
	if (keylist == NULL) return (TRUE);
	if (keylist->list.list_len > row->list.list_len) return (FALSE);
	
	int j, i;
	for (i = 0; i< keylist->list.list_len; i ++) {
		for (j = 0; j < row->list.list_len; j++) {
			if(cmpDmiAttributeData(&(keylist->list.list_val[i]),
								   &(row->list.list_val[j])) == TRUE){
				j = row->list.list_len +1;
			}
		}
		
		if (j == row->list.list_len) return (FALSE);
	}
	return (TRUE);

}

bool_t IsKeyMatch(DmiAttributeValues_t *value, DmiAttributeValues_t *keylist)
{
	return (IsThisRowOnKey(value, (void *)keylist)); 
}


DmiAttributeValues_t *findRow(RWTPtrSlist<DmiAttributeValues_t> *values,
							  DmiAttributeValues_t *keylist)
{
	if ((values == NULL) || (values->entries() == 0)) return (NULL);
	// if keylist is NULL, return first row
	if (keylist == NULL) return  (values->at(0));

	return (values->find(*IsThisRowOnKey, (void *)keylist)); 
}


DmiAttributeValues_t *findNextRow(RWTPtrSlist<DmiAttributeValues_t> *values,
							  DmiAttributeValues_t *keylist)
{
	if ((values == NULL) || (values->entries() == 0)) return (NULL);
	// if keylist is NULL, return first row

	size_t index; 
	if ((keylist == NULL) || (keylist->list.list_len == 0))
		index = 0; 
	else {
		index  = values->index(*IsThisRowOnKey, (void *)keylist);
		if (index == RW_NPOS) return NULL;
	}
	index++; 
	if (index >= values->entries()) return (NULL); 
	return (values->at(index)); 
}

bool_t insertComp(RWTIsvSlist<Component> *comps, Component* comp)
{
	if ((comps == NULL) || (comp== NULL)) return (FALSE);
	
	size_t index =
		comps->index(AfterThisComponent, (void *)(comp->GetComponentId()));
	if (index == RW_NPOS)
		comps->append(comp);
	else
		comps->insertAt(index, comp);
	return (TRUE); 
}

bool_t insertGroup(RWTIsvSlist<Group> *groups, Group* group)
{
	if ((groups == NULL) || (group== NULL)) return (FALSE);
	
	size_t index =
		groups->index(AfterThisGroup, (void *)(group->GetGroupInfo()->id));
	if (index == RW_NPOS)
		groups->append(group);
	else
		groups->insertAt(index, group);
	return (TRUE); 
}

bool_t insertAttribute(RWTIsvSlist<Attribute> *attrs, Attribute* attr)
{
	if ((attrs == NULL) || (attr== NULL)) return (FALSE);
	
	size_t index =
		attrs->index(AfterThisAttr, (void *)(attr->GetId()));
	if (index == RW_NPOS)
		attrs->append(attr);
	else
		attrs->insertAt(index, attr);
	return (TRUE); 
}

	
