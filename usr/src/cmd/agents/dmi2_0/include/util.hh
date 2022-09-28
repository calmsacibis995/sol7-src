
#ifndef _UTIL_HH
#define _UTIL_HH
/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)util.hh	1.20 96/10/07 Sun Microsystems"

#include "common.h"
#ifdef __cplusplus
extern "C" {
#endif 	


extern void printDmiString(DmiString *dstr); 
extern int cmpDmiString(DmiString *str1, DmiString *str2); 
extern int cmpcaseDmiString(DmiString *str1, DmiString *str2);

extern int copyString(DmiString_t *dstr, char *str); 
extern DmiString_t *newDmiString(char *str);
extern DmiTimestamp_t *newDmiTimestampFromDmiTimestamp(DmiTimestamp_t *timestamp);
extern DmiOctetString_t *newDmiOctetString(DmiOctetString_t *str); 
extern DmiOctetString_t *newDmiOctetStringFromString(char *str);
extern DmiString_t *newDmiStringFromDmiString(DmiString_t *str);
extern void freeDmiString(DmiString_t *dstr); 
extern void freeDmiOctetString(DmiOctetString_t *dstr); 
extern int copyCompInfo(DmiComponentInfo_t *, DmiComponentInfo_t *);
extern int printCompList(DmiComponentList_t *complist);
extern int printGroupList(DmiGroupList_t *groups); 
extern int printAttrList(DmiAttributeList *attrs);
extern int copyGroupInfo(DmiGroupInfo_t *, DmiGroupInfo_t *);
extern int copyAttrInfo(DmiAttributeInfo_t *,  DmiAttributeInfo_t *);
extern int copyClassNameFromGroup(DmiClassNameInfo_t *, DmiGroupInfo_t*);
extern int printClassNameList(DmiClassNameList_t *namelist);

extern int
copyClassNameFromGroup(DmiClassNameInfo_t *classinfo, DmiGroupInfo_t *groupinfo); 

extern DmiAttributeValues_t *newDmiAttributeValues(int);
extern DmiAttributeValues_t *newDmiAttributeValuesFromValues(DmiAttributeValues_t *);
extern void freeDmiAttributeValues(DmiAttributeValues_t *d); 
extern bool_t cmpDmiAttributeValues(DmiAttributeValues_t *v1, DmiAttributeValues_t *v2);

extern bool_t cmpDmiAttributeData(DmiAttributeData_t *d1, DmiAttributeData_t *d2);
extern DmiAttributeData_t *newDmiAttributeData(); 
extern void freeDmiAttributeData(DmiAttributeData_t *data); 
extern void printDmiAttributeData(DmiAttributeData_t *data); 
extern void printDmiDataUnion(DmiDataUnion_t *data);
extern void copyDmiAttributeData(DmiAttributeData_t *d1, DmiAttributeData_t *d2 );
extern void copyDmiDataUnion(DmiDataUnion_t *d1, DmiDataUnion_t *d2 );
extern void printDmiMultiRowData(DmiMultiRowData_t *mrow);

extern void printDmiTimestamp(DmiTimestamp_t *timestamp);
extern void setCurrentTimeStamp(DmiTimestamp_t *timestamp); 
extern void freeDmiMultiRowData (DmiMultiRowData_t *md);
extern DmiAttributeIds_t *newDmiAttributeIds(int length);
extern DmiAttributeIds_t *newDmiAttributeIdsFromIds(DmiAttributeIds_t *ids); 
extern void freeDmiAttributeIds(DmiAttributeIds_t *ids); 

#ifdef __cplusplus
}
#endif 	
#endif
