// Copyright 11/01/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)initialization.cc	1.27 96/11/01 Sun Microsystems"


#include <stdlib.h>
#include <dirent.h>
#include "dmi.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "mutex.hh"
#include "subscription.hh"
#include "dbapi.hh"
#include "trace.hh"
#include "initialization.hh"
//
// global variables
//

DmiInfo_t dmiinfo;
ParseErr *parseErr= NULL;

//#define theComponents dmiinfo.components
DmiId_t ComponentID = 1;
char DMI_SPECLEVEL[] = "Dmi2.0\n";
char DMI_DESC[] = "This is a DMI2.0 based on ONC RPC\n";
char DMI_LANG[] = "English\n";
char DMI_FILE[] = "TEXT FILE\n";
DmiFileType DMI_FILE_TYPES[] = {
	DMI_FILETYPE_0,
	DMI_FILETYPE_1
};

extern void server_svc();

void InitConfig(char *conf_dir, char *dbdir, char *mifdir)
{
	char *filename;
	filename = (char *)malloc (strlen(conf_dir) + 20);
	sprintf(filename, "%s/dmispd.conf", conf_dir);
	trace("read config file %s\n", filename); 
	FILE *fp = fopen(filename, "r");
	if (fp == NULL) {
		error("Cannot open %s\n", filename);
		free(filename);
		exit(1);
	}

	char mode[64];
	char path[124];
	char line[124];

	while (fgets(line, 120, fp) != NULL) {
		if (line[0] != '#') {
			sscanf(line, "%s %s", mode, path);
			if (!strcmp(mode, "DBDIR")) 
				memcpy(dbdir,path, strlen(path)+1);
			if (!strcmp(mode, "MIFDIR"))
				memcpy(mifdir,path, strlen(path)+1);
		}
	}

	free(filename);	
	fclose(fp); 
}

void InitDmiInfo(char *config_dir)
{
	char dbdir[128], mifdir[128];
	int err; 
	InitConfig(config_dir, dbdir, mifdir);
	
	DIR *dirp ; 
	if ((dirp = opendir(dbdir)) == NULL) {
		error("cannot set DBDIR to %s, please check the configuration file %s/dmispd.conf. \n",
			  dbdir, config_dir);
		exit(1);
	}
	closedir(dirp); 

	if ((dirp = opendir(mifdir)) == NULL) {
		error("cannot set MIFDIR to %s, please check the configuration file %s/dmispd.conf. \n",
			  mifdir, config_dir);
		exit(1);
	}
	closedir(dirp); 

	parseErr = new ParseErr(); 
	InitDMIDBServer(err, dbdir, mifdir);
	if (err != DMIERR_NO_ERROR) {
		error("unable to init DB, exit, check the permission.\n");
		exit(1);
	}
	
	trace("DBDIR = %s\nMIFDIR = %s \n", dbdir, mifdir);
	
	init_component_lock();
	RWTIsvSlist<Component> *comps = ReadAllComponentsFromDB(err);
//	if (err != DMIERR_NO_ERROR) {
//		error("Database reading error, exit, check the permission.\n");
//		exit(1);
//	}
	DmiId_t spId = 1;
	Component *thisComp; 
	if ((comps == NULL) ||
		(comps->entries() == 0)) {
		thisComp = CreateComponentFromMif(err, "sp.mif", 1);
		if (thisComp == NULL) {
			error("error: install sp.mif, exit"); 
			exit(1); 
		}

		dmiinfo.components = new RWTIsvSlist<Component>(); 
		dmiinfo.components->append(thisComp);
		WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
		if (err != DMIERR_NO_ERROR) {
			error("Database writing error, exit, check the permission.\n");
			exit(1);
		}

	}
	else {
		thisComp = comps->last();
		ComponentID = thisComp->GetComponentId();
		trace("Init ComponentId : %d \n", ComponentID); 
		if (comps->find(*IsThisComponent, (void *) spId )== NULL) {
			thisComp = CreateComponentFromMif(err, "sp.mif", spId); 
			if (thisComp == NULL) {
				error("error: install sp.mif !, exit \n"); 				
				exit(1); 
			}
			comps->insertAt(0, thisComp);
			WriteComponentToDB(err, thisComp, DB_UPDATE_ALL);
			if (err != DMIERR_NO_ERROR) {
				error("Database writing error, exit, check the permission.\n");
				exit(1);
			}
		}
		dmiinfo.components = comps;
	}
	dmiinfo.dmiSpecLevel = newDmiString(DMI_SPECLEVEL);
	dmiinfo.description = newDmiString(DMI_DESC);
	dmiinfo.fileTypes = NULL; 
	dmiinfo.language= newDmiString(DMI_LANG);
	event_subscriber_init();
	server_svc(); 
}

bool_t
dmiregister(DmiRegisterIN *argp, DmiRegisterOUT *result)
{
	result->handle = &(argp->handle);
	result->error_status = DMIERR_NO_ERROR; 
	return (TRUE);
}

bool_t
dmiunregister(DmiUnregisterIN *argp, DmiUnregisterOUT *result)
{
	result->error_status = DMIERR_NO_ERROR; 
	return (TRUE);
}

bool_t
dmigetversion(DmiGetVersionIN *argp, DmiGetVersionOUT *result)
{
	acquire_component_lock(); 
	result->error_status = DMIERR_NO_ERROR;
	result->dmiSpecLevel = dmiinfo.dmiSpecLevel;
	result->description = dmiinfo.description;
	result->fileTypes = dmiinfo.fileTypes;
	release_component_lock(); 
	return (TRUE); 
}

bool_t
dmigetconfig(DmiGetConfigIN *argp, DmiGetConfigOUT *result)
{
	acquire_component_lock(); 
	result->error_status = DMIERR_NO_ERROR;
	result->language = dmiinfo.language;
	release_component_lock();
	return (TRUE); 
}

bool_t 
dmisetconfig(DmiSetConfigIN *argp, DmiSetConfigOUT *result)
{
	acquire_component_lock();
	if (argp->language == NULL) {
		result->error_status = DMIERR_ILLEGAL_TO_SET;
		return (FALSE);
	}
	result->error_status = DMIERR_NO_ERROR;
	if (dmiinfo.language != NULL) {
		freeDmiString(dmiinfo.language);
	}
	
	dmiinfo.language = newDmiStringFromDmiString(argp->language);
	release_component_lock();
	return (TRUE);
}

