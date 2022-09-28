
// Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dbapi.hh	1.8 96/09/24 Sun Microsystems"

#ifndef _DBAPI_HH_
#define _DBAPI_HH_

#include "dmi.hh"

typedef enum {
 	DB_UPDATE_ALL=0,
	DB_UPDATE_COMP_ONLY,
	DB_UPDATE_TABLE_ONLY
} DB_Comp_Update;

/*
 * Input: filename and compId are input arguments
 * Alg: check whether the db is initialized or not
 *      check whether the file filenamecompId.xxx exists or not
 * 	if not, return NULL.
 *	open then read the given
 *	MIF file, during parsing create the component
 *	object. If object is created successfully, write it
 *	to the filenamecompId.comp and filenamecompId.tbl. If
 *	the name exists, try other names.
 *	if no parsing error, return the Component object
 */
/*
 * Remark: should return the Component object
 */
extern Component* CreateComponentFromMif(int &opRes,char* miffilename, DmiId_t compId);
/*extern Component* CreateComponentFromMif(char* miffilename, DmiId_t compId);*/


/*
 * Purpose: read a group file, create/append the group info to the
 * 	    existing component file. Here, assume that the duplication
 *	    of group is checked before calling this function.
 *
 * Input: filename is the name of the group file
 *	  compfilesuffix is the name of the component file suffix(xxxcompId)
 *	  groupId is the default group id, which is used when no group id
 *	  exists in the group file
 *	
 * Output: return Group object if no error in fileparsing and storing.
 */
extern Group* CreateGroup(int &opRes,char* grpfilename, DmiId_t groupId);
/*extern Group* CreateGroup(char* grpfilename, DmiId_t groupId);*/


/*
 * Purpose: parsing all the *.comp and *.tbl files under the given directory
 *
 * Input: dirname is the directory name storing all the data files
 * Output: return the list of components if no parsing error
 * 	   otherwise, return NULL
 */
extern RWTIsvSlist<Component>* ReadAllComponentsFromDB(int &opRes);
/*extern RWTIsvSlist<Component>* ReadAllComponentsFromDB();*/

extern Component* ReadComponentFormDB(int &opRes,DmiId_t compId);
/*extern Component* ReadComponentFormDB(DmiId_t compId);*/

/*
 * Purpose: update/overwrite the component info into the database
 *
 * Input: the component objec
 *
 * Alg: create a temporary comp file and tble file storing the comp snapshort
 *   	streaming the component object into the files. If successful, then
 *	close the files in the given comp if exist.  rename the temporary file
 *	to the existing file name or generated filename.
 * 
 * Output: return TRUE if successful
 */
extern bool_t WriteComponentToDB(int &opRes,Component *comp,DB_Comp_Update flag);
/*extern bool_t WriteComponentToDB(Component *comp,DB_Comp_Update flag);*/


/* 
 * Purpose: delete the component info/data from the database
 * Input: the suffix of the component(eg. compnameCompId)
 * Alg: remove the xxx.comp and xxx.tbl files
 * Output: return TRUE if successful, otherwise FALSE
 */
extern bool_t DeleteComponentFormDB(int &opRes,DmiId_t compId);
/*extern bool_t DeleteComponentFormDB(DmiId_t compId);*/



/*
 * Purpose: intialize the database
 * Input: dirname is the directory storing the database
 * Output: if successful, return TRUE, otherwise error
 */
extern bool_t InitDMIDBServer(int &opRes,char* dirname,char* MifDir=0);
/*extern bool_t InitDMIDBServer(char* dirname,char* MifDir);
extern bool_t InitDMIDBServer(char* dirname);*/

extern DmiString* GetLanguageFromMif(int &opRes,char* miffilename);

#endif
