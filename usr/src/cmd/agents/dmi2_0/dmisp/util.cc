// Copyright 04/24/97 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)util.cc	1.25 97/04/24 Sun Microsystems"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "dmi.hh"
#include "util.hh"
#include "trace.hh"


int cmpDmiString(DmiString *str1, DmiString *str2)
{
	if ((str1 == NULL ) || (str2 == NULL)) return (-1);
	if (str1->body.body_len != str2->body.body_len ) return (-1);
	return (strncmp(str1->body.body_val,
					str2->body.body_val,
					str1->body.body_len)); 
}

int cmpcaseDmiString(DmiString *str1, DmiString *str2)
{
	if ((str1 == NULL ) || (str2 == NULL)) return (-1);
	if (str1->body.body_len != str2->body.body_len ) return (-1);
	return (strncasecmp(str1->body.body_val,
					str2->body.body_val,
					str1->body.body_len)); 
}

void printDmiString(DmiString *dstr)
{
	if (dstr == NULL) {
		cout << "";
		return;
	}

	for (int i = 0; i< dstr->body.body_len; i++)
		printf("%c", dstr->body.body_val[i]);
	fflush(0); 
}

int copyString(DmiString_t *dstr, char *str)
{
	if ((dstr == NULL) || (str == NULL)) return (-1);

	dstr->body.body_len = strlen (str);
	dstr->body.body_val = str;

	return (0); 
}


//allocate memory for a DmiString

DmiString_t *newDmiStringFromDmiString(DmiString_t *str)
{
	if (str == NULL) return (NULL);
	
	DmiString_t *dstr = (DmiString_t *) malloc (sizeof(DmiString_t));
	
	if (dstr == NULL ) {
		printf("malloc dstr: Out of memory\n");
		return (NULL); 
	}
	dstr->body.body_len = str->body.body_len;
	dstr->body.body_val = (char *) malloc (dstr->body.body_len);
	if (dstr->body.body_val == NULL) {
		printf("malloc dstr->body.body_val: Out of memory\n");
		return (NULL);
	}
	memcpy(dstr->body.body_val, str->body.body_val, dstr->body.body_len); 
	return (dstr); 
}

void printDmiOctetString(DmiOctetString *dstr)
{
	if (dstr == NULL) {
		cout << "";
		return;
	}

	for (int i = 0; i< dstr->body.body_len; i++)
		printf("%c", dstr->body.body_val[i]);
	fflush(0); 
}

int cmpDmiOctetString(DmiOctetString *str1, DmiOctetString *str2)
{
	if ((str1 == NULL ) || (str2 == NULL)) return (-1);
	if (str1->body.body_len != str2->body.body_len ) return (-1);
	return (strncmp(str1->body.body_val,
					str2->body.body_val,
					str1->body.body_len)); 
}


DmiOctetString_t *newDmiOctetString(DmiOctetString_t *str)
{
	if (str == NULL) return (NULL);
	
	DmiOctetString_t *dstr = (DmiOctetString_t *) malloc (sizeof(DmiOctetString_t));
	
	if (dstr == NULL ) {
		printf("malloc octet str: Out of memory\n");
		return (NULL); 
	}
	dstr->body.body_len = str->body.body_len;
	dstr->body.body_val = (char *) malloc (dstr->body.body_len);
	if (dstr->body.body_val == NULL) {
		printf("malloc dstr->body.body_val: Out of memory\n");
		return (NULL);
	}
	memcpy(dstr->body.body_val, str->body.body_val, dstr->body.body_len); 
	return (dstr); 
}

DmiOctetString_t *newDmiOctetStringFromString(char *str)
{
	if (str == NULL) return (NULL);
	
	DmiOctetString_t *dstr = (DmiOctetString_t *) malloc (sizeof(DmiOctetString_t));
	
	if (dstr == NULL ) {
		printf("malloc octet str: Out of memory\n");
		return (NULL); 
	}
	dstr->body.body_len = strlen(str);
	dstr->body.body_val = (char *) malloc (dstr->body.body_len);
	if (dstr->body.body_val == NULL) {
		printf("malloc dstr->body.body_val: Out of memory\n");
		return (NULL);
	}
	memcpy(dstr->body.body_val, str, dstr->body.body_len); 
	return (dstr); 
}


DmiString_t *newDmiString(char *str)
{
	DmiString_t *dstr = (DmiString_t *) malloc (sizeof(DmiString_t));
	
	if (dstr == NULL ) {
		printf("malloc dstr: Out of memory\n");
		return (NULL); 
	}
	dstr->body.body_len = strlen(str);
	dstr->body.body_val = (char *) malloc (dstr->body.body_len);
	if (dstr->body.body_val == NULL) {
		printf("malloc dstr->body.body_val: Out of memory\n");
		return (NULL);
	}
	memcpy(dstr->body.body_val, str, dstr->body.body_len); 
	return (dstr); 
}

void freeDmiString(DmiString_t *dstr)
{
	if (dstr == NULL) return;
	if (dstr->body.body_val != NULL) {
		free (dstr->body.body_val);
		dstr->body.body_val = NULL;
		dstr->body.body_len = 0; 
	}
	free(dstr);
	dstr = NULL;
}

void freeDmiOctetString(DmiOctetString_t *dstr)
{
	if (dstr == NULL) return;
	if (dstr->body.body_val != NULL) {
		free (dstr->body.body_val);
		dstr->body.body_val = NULL;
		dstr->body.body_len = 0; 
	}
	free(dstr);
	dstr = NULL;
}


int copyCompInfo(DmiComponentInfo_t *dcompinfo, DmiComponentInfo_t *scompinfo)
{
	if ((dcompinfo == NULL) || (scompinfo == NULL)) return (-1); 
	dcompinfo->id = scompinfo->id;
	dcompinfo->name = scompinfo->name ;
	dcompinfo->pragma = scompinfo->pragma;
	dcompinfo->description = scompinfo->description;
	dcompinfo->exactMatch = scompinfo->exactMatch;
	return (0); 
}

int printCompList(DmiComponentList_t *complist)
{
	if (complist== NULL) return (-1); 
	for (int i = 0; i < complist->list.list_len; i++) {
		cout << complist->list.list_val[i].id << "\t" ;
		printDmiString(complist->list.list_val[i].name);
		printDmiString(complist->list.list_val[i].pragma);
		printDmiString(complist->list.list_val[i].description);
		cout << complist->list.list_val[i].exactMatch;
		cout << endl; 
	}
	return (0); 
}

int printGroupList(DmiGroupList_t *groups)
{
	if (groups == NULL) return (-1);
	for (int i = 0; i< groups->list.list_len; i++) {
		cout << groups->list.list_val[i].id << "\t" ;
		printDmiString(groups->list.list_val[i].name);
		printDmiString(groups->list.list_val[i].pragma);
		printDmiString(groups->list.list_val[i].description);
//		cout << groups->list.list_val[i].exactMatch;
		cout << endl; 
	}
	return (0); 
}

int printAttrList(DmiAttributeList *attrs)
{
	if (attrs == NULL) return (-1);
	for (int i = 0; i< attrs->list.list_len; i++) {
		cout << attrs->list.list_val[i].id << "\t" ;
		printDmiString(attrs->list.list_val[i].name);
		printDmiString(attrs->list.list_val[i].pragma);
		printDmiString(attrs->list.list_val[i].description);
		cout << endl;
		fflush(0); 
	}
	return (0); 
}



int copyGroupInfo(DmiGroupInfo_t *dg, DmiGroupInfo_t *sg)
{
	if ((dg == NULL) || (sg== NULL)) return (-1); 
	dg->id = sg->id;
	dg->name = sg->name;
	dg->pragma = sg->pragma;
	dg->className= sg->className;
	dg->description = sg->description;
	dg->keyList = sg->keyList;

	return (0); 
}
	
	
int copyAttrInfo(DmiAttributeInfo_t *da,  DmiAttributeInfo_t *sa)
{
	if ((da == NULL) || (sa== NULL)) return (-1);
	da->id = sa->id;
	da->name = sa->name;
	da->pragma = sa->pragma;
	da->description = sa->description;
	da->storage = sa->storage;
	da->access = sa->access;
	da->type = sa->type;
	da->maxSize = sa->maxSize;
	da->enumList = sa->enumList;
	return (0); 
}

int
copyClassNameFromGroup(DmiClassNameInfo_t *classinfo, DmiGroupInfo_t *groupinfo)
{
	if ((classinfo == NULL) || (groupinfo == NULL)) return (-1);
	classinfo->id = groupinfo->id;
	classinfo->className = groupinfo->className;
	return (0); 
}

int
printClassNameList(DmiClassNameList_t *namelist)
{
	if (namelist == NULL) return (-1);
	for (int i= 0; i< namelist->list.list_len; i++) {
		cout << namelist->list.list_val[i].id << "\t" ;
		printDmiString(namelist->list.list_val[i].className);
		cout << endl; 
	}
	return (0); 
}

bool_t cmpDmiTimestamp(DmiTimestamp_t *d1, DmiTimestamp_t *d2)
{
	if ((d1 == NULL) || (d2 == NULL)) return FALSE;

	// compare
	return TRUE; 
}

DmiTimestamp_t *newDmiTimestampFromDmiTimestamp(DmiTimestamp_t *timestamp)
{
	if (timestamp == NULL) return (NULL); 
	DmiTimestamp_t *result = (DmiTimestamp_t *)
		malloc (sizeof(DmiTimestamp_t));

	if (result == NULL) {
		error("error malloc in newDmiTimeStamp \n"); 
		return NULL;
	}
	memcpy(result, timestamp, sizeof(DmiTimestamp_t)); 
	
	return (result);
}

bool_t cmpDmiAttributeData(DmiAttributeData_t *d1, DmiAttributeData_t *d2)
{
	if ((d1== NULL) || (d2 == NULL)) return FALSE;
	if ((d1->id != d2->id) || (d1->data.type != d2->data.type)) return FALSE;
	switch (d2->data.type) {
		case MIF_COUNTER:
			if (d1->data.DmiDataUnion_u.counter
				== d2->data.DmiDataUnion_u.counter ) return TRUE;
			else
				return FALSE; 
		case MIF_COUNTER64:
			if ((d1->data.DmiDataUnion_u.counter64[0] ==
				 d2->data.DmiDataUnion_u.counter64[0]) &&
				(d1->data.DmiDataUnion_u.counter64[1] ==
				 d2->data.DmiDataUnion_u.counter64[1]))
				return (TRUE); 
			else
				return FALSE; 
		case MIF_GAUGE:
			if (d1->data.DmiDataUnion_u.gauge == d2->data.DmiDataUnion_u.gauge)
				return (TRUE); 
			else
				return FALSE; 
		case MIF_INTEGER:
			if (d1->data.DmiDataUnion_u.integer == d2->data.DmiDataUnion_u.integer)
				return (TRUE); 
			else
				return FALSE; 
		case MIF_INTEGER64:
			if ((d1->data.DmiDataUnion_u.integer64[0] ==
				 d2->data.DmiDataUnion_u.integer64[0]) &&
				(d1->data.DmiDataUnion_u.integer64[1] ==
				 d2->data.DmiDataUnion_u.integer64[1]))
				return (TRUE); 
			else
				return FALSE; 
		case MIF_DATE:
			if (cmpDmiTimestamp(d1->data.DmiDataUnion_u.date,
								d2->data.DmiDataUnion_u.date) == TRUE)
				return (TRUE); 
			else
				return FALSE; 
		case MIF_OCTETSTRING: 
			if (cmpDmiOctetString(d1->data.DmiDataUnion_u.octetstring,
								  d2->data.DmiDataUnion_u.octetstring) == 0)
				return (TRUE); 
			else
				return FALSE; 
		case MIF_DISPLAYSTRING:
			if (cmpDmiString(d1->data.DmiDataUnion_u.str,
							 d2->data.DmiDataUnion_u.str)== 0)
				return (TRUE); 
			else
				return FALSE; 
		default:
			return (FALSE); 
			break;
	}
	
}


DmiAttributeData_t *newDmiAttributeData()
{
	DmiAttributeData_t *result = (DmiAttributeData_t *)
		malloc(sizeof(DmiAttributeData_t));
	return (result); 
}

void freeDmiAttributeData(DmiAttributeData_t *data)
{
	if (data == NULL) return ;

	//have to free some sub structure first
	free(data);
	return; 
}

void printDmiAttributeData(DmiAttributeData_t *data)
{
	if ((data == NULL)  || (data->id == 0)) return;
	cout<< data->id<< endl;
	switch (data->data.type) {
		case MIF_DISPLAYSTRING:
			printDmiString(data->data.DmiDataUnion_u.str);
			break;
		case MIF_INTEGER:
			cout << data->data.DmiDataUnion_u.integer << endl;
			break; 
		default:
			cout << "unkown data" << endl; 
			break;
	}
}
	
void copyDmiDataUnion(DmiDataUnion_t *d1, DmiDataUnion_t *d2 )
{
	if (d1 == NULL) return;
	if (d2 == NULL) {
		d1->type = MIF_DISPLAYSTRING;
		d1->DmiDataUnion_u.str = NULL; 
		return ;
	}
	d1->type = d2->type; 
	switch (d2->type) {
		case MIF_COUNTER:
			d1->DmiDataUnion_u.counter
				= d2->DmiDataUnion_u.counter;
			break; 
		case MIF_COUNTER64:
			d1->DmiDataUnion_u.counter64[0]
				= d2->DmiDataUnion_u.counter64[0]; 
			d1->DmiDataUnion_u.counter64[1]
				= d2->DmiDataUnion_u.counter64[1]; 
			break; 
		case MIF_GAUGE:
			d1->DmiDataUnion_u.gauge
				= d2->DmiDataUnion_u.gauge; 
			break; 
		case MIF_INTEGER:
			d1->DmiDataUnion_u.integer
				= d2->DmiDataUnion_u.integer; 
			break; 
		case MIF_INTEGER64:
			d1->DmiDataUnion_u.integer64[0]
				= d2->DmiDataUnion_u.integer64[0]; 
			d1->DmiDataUnion_u.integer64[1]
				= d2->DmiDataUnion_u.integer64[1]; 
			break; 
		case MIF_DATE:
//			if (d1->DmiDataUnion_u.date != NULL)
//				free(d1->DmiDataUnion_u.date);
			d1->DmiDataUnion_u.date
				= newDmiTimestampFromDmiTimestamp(d2->DmiDataUnion_u.date); 
			break; 
		case MIF_OCTETSTRING:
//			if (d1->DmiDataUnion_u.octetstring != NULL)
//				freeDmiOctetString(d1->DmiDataUnion_u.octetstring);
			d1->DmiDataUnion_u.octetstring = newDmiOctetString
				(d2->DmiDataUnion_u.octetstring);
			break;
		case MIF_DISPLAYSTRING:
//			if (d1->DmiDataUnion_u.str != NULL)
//				free(d1->DmiDataUnion_u.str); 
			d1->DmiDataUnion_u.str = newDmiStringFromDmiString
				(d2->DmiDataUnion_u.str);
			break;
		default:
			trace("unknown data type in copyDmiDataUnion\n"); 
			break;
	}
}
void copyDmiAttributeData(DmiAttributeData_t *d1, DmiAttributeData_t *d2 )
{
	if (d1 == NULL) return;
	if (d2 == NULL) {
		d1->id = 0;
		d1->data.type = MIF_DISPLAYSTRING;
		d1->data.DmiDataUnion_u.str = NULL; 
		return ;
	}
	d1->id = d2->id;
	copyDmiDataUnion(&(d1->data), &(d2->data)); 
}

void printDmiDataUnion(DmiDataUnion_t *data)
{
	if (data == NULL) return ;
	switch (data->type) {
		case MIF_DATATYPE_0:
			cout << endl; 
			break; 
		case MIF_COUNTER:
			cout  << data->DmiDataUnion_u.counter << endl; 
			break; 
		case MIF_COUNTER64:
			cout << data->DmiDataUnion_u.counter64[0] <<
				data->DmiDataUnion_u.counter64[1] << endl; 
			break; 
		case MIF_GAUGE:
			cout << data->DmiDataUnion_u.gauge << endl; 
			break; 
		case MIF_DATATYPE_4:
			cout << endl; 
			break; 
		case MIF_INTEGER64:
			cout << data->DmiDataUnion_u.integer64[0] <<
				data->DmiDataUnion_u.integer64[1] << endl; 
			break; 
		case MIF_OCTETSTRING:
			printDmiOctetString(data->DmiDataUnion_u.octetstring); 
			cout << endl; 
			break; 
		case MIF_DATATYPE_9:
			cout << endl; 
			break; 
		case MIF_DATATYPE_10:
			cout << endl; 
			break; 
		case MIF_DATE:
			printDmiTimestamp(data->DmiDataUnion_u.date); 	
			cout << endl; 
			break; 
		case MIF_DISPLAYSTRING:
			printDmiString(data->DmiDataUnion_u.str);
			cout << endl; 
			break;
		case MIF_INTEGER:
			cout << data->DmiDataUnion_u.integer << endl;
			break; 
		default:
			cout << "unknown data" << endl; 
			break;
	}
}


void printDmiAttributeValues(DmiAttributeValues_t *values)
{
	if ((values== NULL) || (values->list.list_len== 0))
		return;
	for (int i= 0; i< values->list.list_len; i++) {
		printDmiAttributeData(&(values->list.list_val[i]));
		cout << endl;
	}
}

DmiAttributeValues_t *newDmiAttributeValues(int length)
{
	if (length == 0) return NULL; 
	DmiAttributeValues_t *result = (DmiAttributeValues_t *)
		malloc(sizeof(DmiAttributeValues_t));
	if (result == NULL) {
		error("malloc error in newDmiAttributeVallues\n"); 
		return NULL; 
	}

	result->list.list_len = length; 
	result->list.list_val = (DmiAttributeData_t *)
		malloc(sizeof(DmiAttributeData_t)*result->list.list_len);
	if (result->list.list_val == NULL) {
		error("malloc error in newDmiAttributeValues\n"); 
		result->list.list_len = 0;
		free(result);
		return (NULL);
	}
	for (int i = 0; i< result->list.list_len; i++)
	{
		result->list.list_val[i].data.type = MIF_DISPLAYSTRING;
		result->list.list_val[i].data.DmiDataUnion_u.str = NULL;
	}
	return (result); 
}

DmiAttributeValues_t *newDmiAttributeValuesFromValues(DmiAttributeValues_t *values)
{
	if (values == NULL) return (NULL);
	DmiAttributeValues_t *result = (DmiAttributeValues_t *)
		malloc(sizeof(DmiAttributeValues_t));

	result->list.list_len = values->list.list_len;
	if (values->list.list_len == 0) {
		result->list.list_val = NULL;
	}
	result->list.list_val = (DmiAttributeData_t *)
		malloc(sizeof(DmiAttributeData_t)*
			   values->list.list_len);
	for (int i= 0; i< values->list.list_len; i++) {
		copyDmiAttributeData(&(result->list.list_val[i]),
							 &(values->list.list_val[i])); 
	}
	return (result); 
}

bool_t cmpDmiAttributeValues(DmiAttributeValues_t *value1, DmiAttributeValues_t *value2)
{
	if ((value1 == NULL)||(value2 == NULL)) return (FALSE);
	if (value1->list.list_len != value2->list.list_len) return (FALSE);
	
	for (int i = 0; i< value1->list.list_len; i ++) {
		if (cmpDmiAttributeData(&(value1->list.list_val[i]),
								&(value1->list.list_val[i]) )==
			FALSE) {
			return FALSE; 
		}
	}
	return (TRUE);
}

void printDmiRowData(DmiRowData_t *rowdata)
{
	if (rowdata == NULL) return ;
	cout << " component ID: " << rowdata->compId;
	cout << " group ID: " << rowdata->groupId << " className: " ;
	printDmiString(rowdata->className);
	cout << " Attribute values: "; 
	printDmiAttributeValues(rowdata->values); 
	
}

void printDmiMultiRowData(DmiMultiRowData_t *mrow)
{
	if ((mrow == NULL) || (mrow->list.list_len == 0)) return;
	for (int i = 0; i< mrow->list.list_len; i++) {
		printDmiRowData(&(mrow->list.list_val[i]));
		cout << endl;
	}
	
}

void freeDmiAttributeValues(DmiAttributeValues_t *d)
{
	if (d== NULL) return;

	for (int i = 0; i< d->list.list_len; i++) {
		switch (d->list.list_val[i].data.type) {
			case MIF_DATE:
				if (d->list.list_val[i].data.DmiDataUnion_u.date != NULL)
					free(d->list.list_val[i].data.DmiDataUnion_u.date);
				break; 
			case MIF_OCTETSTRING:
				if (d->list.list_val[i].data.DmiDataUnion_u.octetstring != NULL)
					freeDmiOctetString(d->list.list_val[i].data.DmiDataUnion_u.octetstring);
				break;
			case MIF_DISPLAYSTRING:
				if (d->list.list_val[i].data.DmiDataUnion_u.str != NULL)
					freeDmiString(d->list.list_val[i].data.DmiDataUnion_u.str); 
				break;
			default:
				break;
		}

	}
	if (d->list.list_val != NULL)
		free(d->list.list_val);
	free(d);
	d= NULL;
}

#include <sys/types.h>
#include <time.h>
void setCurrentTimeStamp(DmiTimestamp_t *timestamp)
//void setCurrentTimeStamp()
{
	if (timestamp == NULL) return ;
	
	time_t t = time(0);
	struct tm *now = gmtime(&t);
	char str[10]; 
	int  year=1900;

	sprintf(str, "%4d", (year+(now->tm_year)));
	memcpy(timestamp->year, str , 4);
	sprintf(str, "%2d", now->tm_mon);	
	memcpy(timestamp->month, str , 2);
	sprintf(str, "%2d", now->tm_mday);	
	memcpy(timestamp->day, str , 2);
	sprintf(str, "%2d", now->tm_hour);	
	memcpy(timestamp->hour, str , 2);
	sprintf(str, "%2d", now->tm_min);	
	memcpy(timestamp->minutes, str , 2);
	sprintf(str, "%2d", now->tm_sec);	
	memcpy(timestamp->seconds, str , 2);
	timestamp->dot = '.';
	memset(timestamp->microSeconds, '\0', 6);
	timestamp->plusOrMinus = '+';
	memset(timestamp->utcOffset, '\0', 3);
	memset(timestamp->padding, '\0', 3); 
}

void printDmiTimestamp(DmiTimestamp_t *timestamp)
{
	if (timestamp == NULL) return ;

	for (int i = 0; i< 4; i++)
		printf("%c", timestamp->year[i]);

	printf(" %c", timestamp->month[0]);
	printf("%c", timestamp->month[1]);
	
	printf(" %c", timestamp->day[0]);
	printf("%c", timestamp->day[1]);
			   
	printf(" %c", timestamp->hour[0]);
	printf("%c", timestamp->hour[1]);

	printf(" %c", timestamp->minutes[0]);
	printf("%c", timestamp->minutes[1]);

	printf(" %c", timestamp->seconds[0]);
	printf("%c", timestamp->seconds[1]);

	fflush(0); 
	
}


DmiAttributeIds_t *newDmiAttributeIds(int length)
{
	if (length == 0) return (NULL); 
	DmiAttributeIds_t *result = (DmiAttributeIds_t *) malloc
		(sizeof(DmiAttributeIds_t));
	if (result == NULL) {
		error("malloc error in newDmiAttributeIds\n"); 
		return NULL; 
	}
	result->list.list_len = length;
	result->list.list_val = (DmiId_t *)malloc
		(sizeof(DmiId_t) *result->list.list_len);
	if (result->list.list_val == NULL) {
		error("malloc error in newDmiAttributeIds\n"); 
		result->list.list_len = 0;
		free(result);
		return (NULL);
	}
	return (result); 
}

DmiAttributeIds_t *newDmiAttributeIdsFromIds(DmiAttributeIds_t *ids)
{
	if ((ids == NULL)||(ids->list.list_len == 0)) return (NULL); 
	DmiAttributeIds_t *result = (DmiAttributeIds_t *) malloc
		(sizeof(DmiAttributeIds_t));
	if (result == NULL) {
		error("malloc error in newDmiAttributeIdsFromIds\n"); 
		return NULL; 
	}
	result->list.list_len = ids->list.list_len;
	result->list.list_val = (DmiId_t *)malloc
		(sizeof(DmiId_t) *result->list.list_len);
	if (result->list.list_val == NULL) {
		error("malloc error in newDmiAttributeIds\n"); 
		result->list.list_len = 0;
		free(result);
		return (NULL);
	}
	for (int i = 0; i< result->list.list_len; i++){
		result->list.list_val[i] = ids->list.list_val[i];
	}
	return (result); 
}

void freeDmiAttributeIds(DmiAttributeIds_t *ids)
{
	if (ids == NULL) return; 

	if (ids->list.list_val != NULL)
		free(ids->list.list_val);

	free(ids);
	return; 
}
#if 0
void printDmiTimestamp(DmiTimestamp *timestamp)
{
	if (timestamp == NULL) return;
	char str[32];
	memset(str, '\0', 64); 
	memcpy(str, year, 4);
	memcpy(str+4, month, 2); 
	memcpy(str+6, day, 2);
	memcpy(str+8, hour, 2); 
	memcpy(str+10, minutes, 2); 
	memcpy(str+12, seconds, 2);
	memcpy(str+14, dot, 1);
	memcpy(str+15, microSeconds, 6);
	memcpy(str+21, plusOrMinus, 1);
	memcpy(str+22, utcOffset, 3);
	cout << str << endl; 
};
#endif
