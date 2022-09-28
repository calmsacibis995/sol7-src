// Copyright 09/25/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dmicmd.cc	1.9 96/09/25 Sun Microsystems"

#include <stdio.h>
#include <stream.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "miapi.hh"
#include "util.hh"
#include "dmi_error.hh"
#include "trace.hh"

static void printDmiStorage(DmiStorageType storage)
{
	switch (storage) {
		case MIF_COMMON:
			trace("MIF_COMMON");
			break;
		case MIF_SPECIFIC:
			trace("MIF_SPECIFIC");
			break;
		default:
			trace("Unknown storage type");
			break;
	}
}

static void printDmiAccess(DmiAccessMode access)
{

	switch (access) {
		case MIF_UNKNOWN_ACCESS:
			trace("MIF_UNKNOWN_ACCESS"); 
			break; 
		case MIF_READ_ONLY:
			trace("MIF_READ_ONLY"); 
			break; 
		case MIF_READ_WRITE:
			trace("MIF_READ_WRITE"); 
			break; 
		case MIF_WRITE_ONLY:
			trace("MIF_WRITE_ONLY"); 
			break; 
		case MIF_UNSUPPORTED:
			trace("MIF_UNSUPPORTED"); 
			break; 
		default:
			trace("MIF_UNKNOWN_ACCESS"); 
			break; 
	}
}

static void printDmiDataType(DmiDataType type)
{
	switch (type) {
		case MIF_DATATYPE_0:
			trace("MIF_DATATYPE_0"); 
			break; 
		case MIF_COUNTER: 
			trace("MIF_COUNTER"); 
			break; 
		case MIF_COUNTER64:
			trace("MIF_COUNTER64"); 
			break; 
		case MIF_GAUGE:
			trace("MIF_GAUGE"); 
			break; 
		case MIF_DATATYPE_4:
			trace("MIF_DATATYPE_4"); 
			break; 
		case MIF_INTEGER:
			trace("MIF_INTEGER"); 
			break; 
		case MIF_INTEGER64:
			trace("MIF_INTEGER64"); 
			break; 
		case MIF_OCTETSTRING:
			trace("MIF_OCTETSTRING"); 
			break; 
		case MIF_DISPLAYSTRING:
			trace("MIF_DISPLAYSTRING"); 
			break; 
		case MIF_DATATYPE_9:
			trace("MIF_DATATYPE_9"); 
			break; 
		case MIF_DATATYPE_10:
			trace("MIF_DATATYPE_10"); 
			break; 
		case MIF_DATE:
			trace("MIF_DATE"); 
			break; 
		default:
			trace("Unknown mif data type"); 
			break; 
	}	
}

static void usage()
{
    fprintf(stderr, "Usage:\n");

    fprintf(stderr, "dmi_cmd [-s hostname]\t\t\t(default host is local host)\n");
	fprintf(stderr, "-h |\t\t\t\t\t\t\t(print usage)\n"); 
	fprintf(stderr, "-V |\t\t\t\t\t\t\t(get version)\n");
	fprintf(stderr, "-W config |\t\t\t\t\t\t(set config)\n");
	fprintf(stderr, "-X |\t\t\t\t\t\t\t(get config)\n");
	fprintf(stderr, "-CI mif_filename |\t\t\t\t\t(install component)\n");
	fprintf(stderr, "-CL [-c compId] [-r reqMode] [-d] [-p] [-m maxCount] |\t(list components)\n");
	fprintf(stderr, "-CD -c compId |\t\t\t\t\t(delete component)\n");
	fprintf(stderr, "-GI schema_filename -c compId |\t\t\t(install group schema)\n");
	fprintf(stderr, "-GL -c compId [-g groupId] [-r reqMode] [-d] [-p] [-m maxCount] |\n\t\t\t\t\t\t\t(list groups)\n");
	fprintf(stderr, "-GM -c compId [-m maxCount] |\t\t\t\t(list classnames)\n");
	fprintf(stderr, "-GD -c compId -g groupId |\t\t\t\t(delete group)\n");
	fprintf(stderr, "-NI schema_filename -c compId |\t\t\t(install lang schema)\n");
	fprintf(stderr, "-NL -c compId |\t\t\t\t\t(list languages)\n");
	fprintf(stderr, "-ND -c compId -l lang |\t\t\t\t(delete language)\n");
	fprintf(stderr, "-AL -c compId -g groupId [-a attrId] [-r reqMode] [-d] [-p] [-m maxCount] |\n\t\t\t\t\t\t\t(list attributes)\n");
//	fprintf(stderr, "-AM -c <compId> -g <groupId> [-a attrId] [-m maxCount]|\n\t\t\t\t\t(get fisrt row attribute value)\n");
	fprintf(stderr, "\nNote: \n"); 
	fprintf(stderr, "\tcompId, groupId, attrId, and maxCount are positive integers.\n\tdefault values are 0.\n\n");
	fprintf(stderr, "\treqMode is 1, 2, or 3. 1 -- DMI_UNIQUE; 2 -- DMI_FIRST; 3 -- DMI_NEXT.\n\tdefault is DMI_UNIQUE, invalid reqMode will set reqMode to default.\n\n"); 
	fprintf(stderr, "\t-d for displaying description, and -p for displaying pragma.\n\tdefault option is no display for pragma and description.\n\n"); 
	fprintf(stderr, "\tconfig is  a string describing the language required by the management application.\n\n"); 
	exit(0); 
}

main(int argc, char *argv[])
{
    extern char *optarg;
	extern int optind;
	int opt;
	int errflag = 0;

	typedef enum ObjectType {
		NONE,
		COMP, 
		GROUP,
		ATTR,
		LANG
	};
	ObjectType oFlag = NONE;
	
	typedef enum ActionType {
		NOACTION,
		GETVER,
		SETCONF,
		GETCONF,
		DEL, 
		ADD,
		LIST,
		SPEC_LIST,
		SET,
		GET,
		MSET,
		MGET
	};
	ActionType aFlag = NOACTION;


	bool_t apierr; 
	int compId = 0;
	int groupId = 0;
	int attrId = 0;
	int	reqMode = 0;
	int maxCount = 0; 
	char *mifname = NULL;
	char *lang = NULL;
	char *hostname = NULL;

	int dflag = 0, pflag = 0; // do we need to display Description and Pragma 

	optind = 1;

	if (argc == 1)
		usage();

	
	
	/* get command-line options */
//    while ((opt = getopt(argc, argv, "hCGANI:LDc:g:a:r:SEMF")) != EOF) {
    while ((opt = getopt(argc, argv, "hs:VW:XCGANMI:LDc:g:a:l:r:dpm:")) != EOF) {
		switch (opt) {
			case 'h':
				usage(); 
				break;
			case 's':
				hostname = strdup(optarg);
				break; 
			case 'V':
				aFlag = GETVER; 
				break; 
			case 'C':
				oFlag = COMP; 
				break; 
			case 'G':
				oFlag = GROUP; 
				break; 
			case 'A':
				oFlag = ATTR;
				break;
			case 'N':
				oFlag = LANG;
				break;
			case 'I':
				aFlag = ADD; 
				mifname = strdup(optarg);
				break; 
			case 'L':
				aFlag = LIST;
				break;
			case 'D':
				aFlag = DEL;
				break;
			case 'M':
				aFlag = SPEC_LIST;
				break;
			case 'c':
				compId = atoi(optarg);
				break;
			case 'g':
				groupId = atoi(optarg);
				break;
			case 'a':
				attrId = atoi(optarg);
				break;
			case 'l':
				lang = strdup(optarg);
				break;
			case 'r':
				reqMode = atoi(optarg);
				break;
			case 'd':
				dflag = 1;
				break;
			case 'p':
				pflag = 1;
				break;
			case 'm':
				maxCount = atoi(optarg);
				break; 
			case 'W':
				aFlag = SETCONF; 
				lang = strdup(optarg); 
				break;
			case 'X':
				aFlag = GETCONF;
				break;
/*			case 'S':
			aFlag = SET; 
			break;
			case 'E':
			aFlag = GET; 
			break;
			case 'M':
			aFlag = MSET; 
			break;
			case 'F':
			aFlag = MGET;
			break;
			*/
			case '?':		/* usage help */
				errflag++; 
				break; 
		}  /* switch */
	}/* while */

	
	if (errflag)
		usage();
	
	if (optind != argc)
		usage();

	trace_on();

	ConnectIN connectin;
	connectin.servertype = MISERVER;
	connectin.rpctype = ONC;
	connectin.nettype = "netpath";

	if (hostname != NULL) {
		connectin.host = hostname;
		argc -= 2; 
	}
	else {
		connectin.host = "localhost";
	}

	DmiRpcHandle dmi_rpc_handle;


		
	//VERSION

	if ((aFlag == GETVER) &&
		(argc == 2)) {
		// call after argu check
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}

		DmiGetVersionIN getversionIn;
		DmiGetVersionOUT getversionOut;

		getversionIn.handle = 0;


		apierr = DmiGetVersion(getversionIn,
							   &getversionOut, &dmi_rpc_handle);
		if (getversionOut.error_status 	== DMIERR_NO_ERROR) {
			trace("dmispd version:\t"); 
			printDmiString(getversionOut.dmiSpecLevel);
			trace("description:\t"); 
			printDmiString(getversionOut.description);
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0);
	}

	// Config

	if ((aFlag == GETCONF) &&
		(argc == 2)) {
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}

		DmiGetConfigIN getconfIn;
		DmiGetConfigOUT getconfOut;
		getconfIn.handle = 0;
		DmiGetConfig(getconfIn, &getconfOut,  &dmi_rpc_handle); 
		if (getconfOut.error_status != DMIERR_NO_ERROR) {
			trace("Get Config fail with error = ");
			fflush(0); 
			dmi_error(getconfOut.error_status);
		}
		else {
			trace("Config of dmispd:\t");
			printDmiString(getconfOut.language);
			trace("\n"); 
		}

		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0);
	}
	
	if ((aFlag == SETCONF) &&
		(lang != NULL) &&
		(argc == 3)) {
//		trace("set config of dmispd to %s\n", lang);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}

		DmiSetConfigIN setconfIn;
		DmiSetConfigOUT setconfOut;
		setconfIn.handle = 0;
		setconfIn.language = newDmiString(lang); 
		DmiSetConfig(setconfIn, &setconfOut,  &dmi_rpc_handle);
		if (setconfOut.error_status != DMIERR_NO_ERROR) {
			trace("set config %s fail with error = ", lang);
			fflush(0); 
			dmi_error(setconfOut.error_status);
		}
		else {
			trace("\"%s\" is set as dmispd config.\n", lang);
		}
		trace("\n"); 
		DisconnectToServer(&dmi_rpc_handle); 
		return (0);
	}

	// Comp
	// delete comp
	if ((oFlag == COMP) &&
		(aFlag == DEL) &&
		(compId > 0) &&     // only specify compId
		(argc == 4)) {         // no mif file
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
//	    trace("del comp: %d \n", compId);
		DmiDeleteComponentOUT delcompOut;
		DmiDeleteComponentIN delcompIn;
		delcompIn.compId = compId; 
		delcompIn.handle = 0;

		apierr = DmiDeleteComponent(delcompIn, &delcompOut, &dmi_rpc_handle); 
		if (delcompOut.error_status != DMIERR_NO_ERROR) {
			trace("del comp %d fail with error = ", compId);
			fflush(0); 
			dmi_error(delcompOut.error_status);
		}
		else {
			trace("comp %d is uninstalled.\n ", compId); 			
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}

	if ((oFlag == COMP) &&
		(aFlag == ADD) &&
		(mifname != NULL) &&
		(argc == 3)) {
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiAddComponentIN addcompIn;
		DmiAddComponentOUT addcompOut;
		DmiFileDataList_t filedatalist;
		filedatalist.list.list_len = 1;
		filedatalist.list.list_val = (DmiFileDataInfo_t *)
			malloc(sizeof(DmiFileDataInfo_t));
		filedatalist.list.list_val[0].fileType = DMI_MIF_FILE_NAME;
		filedatalist.list.list_val[0].fileData =
			newDmiOctetStringFromString(mifname); 
		addcompIn.handle = 0;
		addcompIn.fileData = &filedatalist;

		apierr = DmiAddComponent(addcompIn, &addcompOut, &dmi_rpc_handle); 
		if (addcompOut.error_status == DMIERR_NO_ERROR) {
			trace("\"%s\" is installed as comp %d.\n ",
				  mifname, addcompOut.compId ); 				
		}
		else {
			trace("install \"%s\" fail with error = ", mifname );
			fflush(0); 
			dmi_error(addcompOut.error_status);
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}
	
	if ((oFlag == COMP) &&
		(aFlag == LIST) &&
		(argc <= 10)) {
//		trace("list comps from %d with reqMode %d \n", compId, reqMode);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiListComponentsIN listcompIn; 
		DmiListComponentsOUT listcompOut; 
		listcompIn.handle = 0;

		switch (reqMode) {
			case 1:
				listcompIn.requestMode = DMI_UNIQUE;
				listcompIn.compId = compId; 
				break; 
			case 2:
				listcompIn.requestMode = DMI_FIRST;
				listcompIn.compId = 0; 
				break; 
			case 3:
				listcompIn.requestMode = DMI_NEXT;
				listcompIn.compId = compId; 
				break; 
			default:
				listcompIn.requestMode = DMI_UNIQUE;
				listcompIn.compId = compId; 
				break;
		}
		listcompIn.maxCount = maxCount; 
		listcompIn.getPragma = pflag; 
		listcompIn.getDescription = dflag;
				
		DmiListComponents(listcompIn, &listcompOut, &dmi_rpc_handle);
		if (listcompOut.error_status == DMIERR_NO_ERROR) {
			if (listcompOut.reply != NULL) {
				for (int i = 0; i < listcompOut.reply->list.list_len; i++) {
					trace("\nCompId: \t%d", listcompOut.reply->list.list_val[i].id);
					trace("\nComp Name:\t");
					printDmiString(listcompOut.reply->list.list_val[i].name);
					if (pflag) {
						trace("\nPragma:\t ");
						printDmiString(listcompOut.reply->list.list_val[i].pragma);
					}
					if (dflag) {
						trace("\nDescription:\t"); 
						printDmiString(listcompOut.reply->list.list_val[i].description);
					}
					trace("\n"); 
				}
			}
		}
		else {
			trace("List Components fail with error = ");
			fflush(0); 
			dmi_error(listcompOut.error_status);
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 		
		return (0); 
	}

	//Group
	//Delete Group
	if ((oFlag == GROUP) &&
		(aFlag == DEL) &&
		(compId > 0) &&
		(groupId > 0) &&
		(argc == 6)) {
//		trace("del group %d of comp %d \n", groupId, compId);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiDeleteGroupOUT delgroupOut;
		DmiDeleteGroupIN delgroupIn;
		delgroupIn.compId = compId; 
		delgroupIn.groupId = groupId;
		delgroupIn.handle = 0;
		DmiDeleteGroup(delgroupIn, &delgroupOut, &dmi_rpc_handle);
		if (delgroupOut.error_status != DMIERR_NO_ERROR) {
			trace("del group %d of comp %d fail with error = ",
				  groupId, compId);
			fflush(0); 
			dmi_error(delgroupOut.error_status);
		}
		else {
			trace("group %d of comp %d is deleted.\n ", groupId, compId);
		}

		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}

	// list group
	if ((oFlag == GROUP) &&
		(aFlag == LIST) &&
		(compId > 0) &&		
		(argc <= 12)) {
//		trace("list groups of comp %d from group %d with reqMode %d \n",
//			  compId, groupId, reqMode);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiListGroupsOUT listgroupOut;
		DmiListGroupsIN listgroupIn;
		listgroupIn.compId = compId;
		listgroupIn.groupId = groupId;
		listgroupIn.handle = 0;
				
		listgroupIn.maxCount = maxCount;
		listgroupIn.getPragma = pflag;
		listgroupIn.getDescription = dflag;

		switch (reqMode) {
			case 1:
				listgroupIn.requestMode = DMI_UNIQUE;
				listgroupIn.groupId = groupId; 
				break; 
			case 2:
				listgroupIn.requestMode = DMI_FIRST;

				listgroupIn.groupId = 0; 
				break; 
			case 3:
				listgroupIn.requestMode = DMI_NEXT;
				listgroupIn.groupId = groupId; 
				break; 
			default:
				listgroupIn.requestMode = DMI_UNIQUE;
				listgroupIn.groupId = groupId;
				break;
		}

		DmiListGroups(listgroupIn, &listgroupOut, &dmi_rpc_handle); 
		if (listgroupOut.error_status == DMIERR_NO_ERROR) {
			DmiGroupList_t *groups = listgroupOut.reply;
			trace("List Group for comp %d:\n", compId); 
			if (groups != NULL) {
				for (int i = 0; i< groups->list.list_len; i++) {
					trace("\nGroup ID:\t%d", groups->list.list_val[i].id);
					trace("\nName:\t\t"); 
					printDmiString(groups->list.list_val[i].name);
					if (pflag) {
						trace("\nPragma:\t"); 
						printDmiString(groups->list.list_val[i].pragma);
					}
					if (dflag) {
						trace("\nDescription:\t"); 
						printDmiString(groups->list.list_val[i].description);
					}
					trace("\nClassname:\t"); 
					printDmiString(groups->list.list_val[i].className);
					trace("\nKey:\t"); 
					if ((groups->list.list_val[i].keyList == NULL) ||
						(groups->list.list_val[i].keyList->list.list_len == 0)) {
						trace("no key");
					}
					else {
						for (int j = 0; j < groups->list.list_val[i].keyList->list.list_len; j++)
							trace("%d  ", groups->list.list_val[i].keyList->list.list_val[j]);
					}
					trace("\n"); 
				}
			}
		}
		else {
			trace("list groups of comp %d fail with error = ", compId);
			fflush(0);
			dmi_error(listgroupOut.error_status); 
		}
		
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}

	// list classnames
	
	if ((oFlag == GROUP) &&
		(aFlag == SPEC_LIST) &&
		(compId > 0) &&		
		(argc <= 6)) {
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiListClassNamesOUT namesOut;
		DmiListClassNamesIN namesIn; 
		namesIn.compId = compId;
		namesIn.handle = 0;
		namesIn.maxCount = maxCount;
		DmiListClassNames(namesIn, &namesOut, &dmi_rpc_handle); 
		if (namesOut.error_status != DMIERR_NO_ERROR) {
			trace("list classname for comp %d fail with error = ", compId);
			fflush(0); 
			dmi_error(namesOut.error_status);
		}
		else {
			trace("Classnames in comp %d:\n", compId); 
			DmiClassNameList_t *namelist = namesOut.reply; 
			if (namelist != NULL) {
				for (int i= 0; i< namelist->list.list_len; i++) {
					trace("\nGroup ID:\t%d", namelist->list.list_val[i].id);
					trace("\nClassname:\t"); 
					printDmiString(namelist->list.list_val[i].className);
					trace("\n"); 
				}
			}
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle);
		return (0); 
	}
	
	// install group
	if ((oFlag == GROUP) &&
		(aFlag == ADD) &&
		(compId > 0) &&		
		(mifname != NULL) &&
		(argc == 5)) {
//		trace("add group from %s into comp %d \n",
//			  mifname, compId);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiAddGroupIN addgroupIn;
		DmiAddGroupOUT addgroupOut;
		DmiFileDataList_t filedatalist;
		addgroupIn.compId = compId; 

		filedatalist.list.list_len = 1;
		filedatalist.list.list_val = (DmiFileDataInfo_t *)
			malloc(sizeof(DmiFileDataInfo_t)*
				   filedatalist.list.list_len);
		filedatalist.list.list_val[0].fileType = DMI_MIF_FILE_NAME;
		filedatalist.list.list_val[0].fileData =
			newDmiOctetStringFromString(mifname); 
		addgroupIn.handle = 0;
		addgroupIn.fileData = &filedatalist;

		DmiAddGroup(addgroupIn, &addgroupOut, &dmi_rpc_handle);
		if (addgroupOut.error_status == DMIERR_NO_ERROR) {
			trace("\"%s\" is install as group %d of comp %d\n",
				  mifname, addgroupOut.groupId, compId); 
		}
		else {
			trace("Install %s in comp %d fail with error = ",
				  mifname, compId);
			fflush(0);
			dmi_error(addgroupOut.error_status); 
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}

	//Attr
#if 0	
	if ((oFlag == ATTR) &&
		(aFlag == SPEC_LIST) &&
		(compId > 0) &&
		(groupId > 0) &&
		(argc <= 10)) {
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}


		DmiListAttributesIN listattIn;
		DmiListAttributesOUT listattOut;
		listattIn.handle = 0;
		listattIn.requestMode = DMI_UNIQUE;
		listattIn.maxCount = maxCount;
		listattIn.getPragma = pflag;
		listattIn.getDescription = dflag;
		listattIn.compId = compId; 
		listattIn.groupId = groupId;
		listattIn.attribId = attrId; 
		DmiListAttributes(listattIn, &listattOut, &dmi_rpc_handle);
		if (listattOut.error_status != DMIERR_NO_ERROR) {
			trace("get attrs in group %d of comp %d fail with \nerror = ",
				  groupId, compId);
			fflush(0); 
			dmi_error(listattOut.error_status);
		}
		else {
			DmiAttributeList *attrs = listattOut.reply;
			if (attrs != NULL) {
				DmiGetAttributeOUT getattOut;
				DmiGetAttributeIN getattIn;
				trace("In Group %d of comp %d:\n", groupId, compId); 
				for (int i = 0; i< attrs->list.list_len; i++) {
					attrId =  attrs->list.list_val[i].id; 
					getattIn.handle =0;
					getattIn.compId = compId; 
					getattIn.groupId = groupId; 
					getattIn.attribId = attrId; 
					getattIn.keyList = NULL;
					DmiGetAttribute(getattIn, &getattOut, &dmi_rpc_handle);
					if (getattOut.error_status != DMIERR_NO_ERROR) {
						trace("Get attr %d of group %d of comp %d fail with error = ",
							  attrId, groupId, compId);
						fflush(0); 
						dmi_error(getattOut.error_status);
					}
					else {
						trace("\nAttr ID:\t%d", attrId); 
						if (getattOut.value != NULL) {
							trace("\nType:\t\t");
							printDmiDataType(getattOut.value->type);
							trace("\nValue:\t\t");
							printDmiDataUnion(getattOut.value);
						}
						trace("\n"); 
					}
				}
			}
		}
		
		
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}
#endif
	
	if ((oFlag == ATTR) &&
		(aFlag == LIST) &&
		(compId > 0) &&
		(groupId > 0) &&		
		(argc <=14)) {
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}

		DmiListAttributesIN listattIn;
		DmiListAttributesOUT listattOut;
		listattIn.handle = 0;
		listattIn.requestMode = DMI_UNIQUE;
		listattIn.maxCount = maxCount;
		listattIn.getPragma = pflag;
		listattIn.getDescription = dflag;
		listattIn.compId = compId; 
		listattIn.groupId = groupId; 

		switch (reqMode) {
			case 1:
				listattIn.requestMode = DMI_UNIQUE;
				listattIn.attribId = attrId; 
				break; 
			case 2:
				listattIn.requestMode = DMI_FIRST;
				listattIn.attribId = 0; 
				break; 
			case 3:
				listattIn.requestMode = DMI_NEXT;
				listattIn.attribId = attrId; 
				break; 
			default:
				listattIn.requestMode = DMI_UNIQUE;
				listattIn.attribId = attrId; 
				break;
		}
		
		DmiListAttributes(listattIn, &listattOut, &dmi_rpc_handle);
		if (listattOut.error_status != DMIERR_NO_ERROR) {
			trace("list attr in group %d of comp %d fail with \nerror = ",
				  groupId, compId);
			fflush(0); 
			dmi_error(listattOut.error_status);
		}
		else {
			DmiAttributeList *attrs = listattOut.reply;
			if (attrs != NULL) {
				trace("%d attrs listed for group %d of comp %d\n",
					  attrs->list.list_len, groupId, compId); 
				for (int i = 0; i< attrs->list.list_len; i++) {
					trace("\nAttr Id:\t%d", attrs->list.list_val[i].id);
					trace("\nName:\t\t"); 
					printDmiString(attrs->list.list_val[i].name);
					if (pflag) {
						trace("\nPragma:\t"); 
						printDmiString(attrs->list.list_val[i].pragma);
					}
					if (dflag) {
						trace("\nDescription:\t"); 
						printDmiString(attrs->list.list_val[i].description);
					}
					trace("\nStorage:\t");
					printDmiStorage(attrs->list.list_val[i].storage);
					trace("\nAccess:\t\t");
					printDmiAccess(attrs->list.list_val[i].access);
					trace("\nType:\t\t");
					printDmiDataType(attrs->list.list_val[i].type);
					trace("\nmaxSize:\t%d", attrs->list.list_val[i].maxSize);
//	DmiEnumList_t attrs->list.list_val[i].enumList;
					
					trace("\n"); 

				}
			}
			else {
				trace("0 attrs listed for group %d of comp %d\n",
					  groupId, compId); 
			}
		}
		
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}


	//Lang
	if ((oFlag == LANG) &&
		(aFlag == DEL) &&
		(compId > 0) &&
		(lang != NULL) &&
		(argc == 6)) {
//		trace("del lang %s from comp %d \n", lang, compId);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiDeleteLanguageIN dellangIn;
		DmiDeleteLanguageOUT dellangOut;
		dellangIn.handle = 0;
		dellangIn.compId = compId;
		dellangIn.language = newDmiString(lang);
		DmiDeleteLanguage(dellangIn, &dellangOut, &dmi_rpc_handle); 
		if (dellangOut.error_status != DMIERR_NO_ERROR) {
			trace("Del lang %s in comp %d fail with error = ",
				  lang, compId);
			fflush(0); 
			dmi_error(dellangOut.error_status);
		}
		else {
			trace("lang %s in comp %d is deleted",
				  lang, compId);
		}

		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}
	if ((oFlag == LANG) &&
		(aFlag == LIST) &&
		(compId > 0) &&		
		(argc <= 6)) {
//		trace("list langs of comp %d \n",  compId);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}
		DmiListLanguagesIN listlangIn;
		DmiListLanguagesOUT listlangOut;
		listlangIn.handle = 0;
		listlangIn.maxCount = maxCount;
		listlangIn.compId = compId;

		DmiListLanguages(listlangIn, &listlangOut, &dmi_rpc_handle);
		if (listlangOut.error_status != DMIERR_NO_ERROR) {
			trace("list lang for comp %d fail with error = ", compId);
			fflush(0); 
			dmi_error(listlangOut.error_status);
		}
		else {
			trace("langs of comp %d:\n", compId); 
			if (listlangOut.reply != NULL) {
				for ( int i= 0; i< listlangOut.reply->list.list_len; i++) {
					printDmiString(listlangOut.reply->list.list_val[i]);
					trace("\n"); 
				}
			}
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}

	if ((oFlag == LANG) &&
		(aFlag == ADD) &&
		(compId > 0) &&		
		(mifname != NULL) &&
		(argc == 5)) {
//		trace("add lang from %s into comp %d \n", mifname, compId);
		trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
			trace("Fail to connect to %s\n", connectin.host); 
			exit(-1);
		}

		DmiAddLanguageOUT addlangOut;
		DmiAddLanguageIN addlangIn;
		DmiFileDataList_t filedatalist;

		filedatalist.list.list_len = 1;
		filedatalist.list.list_val = (DmiFileDataInfo_t *)
			malloc(sizeof(DmiFileDataInfo_t)*
				   filedatalist.list.list_len);
		filedatalist.list.list_val[0].fileType = DMI_MIF_FILE_NAME;
		filedatalist.list.list_val[0].fileData =
			newDmiOctetStringFromString(mifname); 
				
		addlangIn.handle = 0;
		addlangIn.compId = compId;
		addlangIn.fileData = &filedatalist;

		DmiAddLanguage(addlangIn, &addlangOut, &dmi_rpc_handle);
		if (addlangOut.error_status != DMIERR_NO_ERROR) {
			trace("install \"%s\" in comp %d fail with error = ",
				  mifname, compId);
			fflush(0);
			dmi_error(addlangOut.error_status);
		}
		else {
			trace("lang schema in file \"%s\" is installed in comp %d\n",
				  mifname, compId);
		}
		trace("\n");
		DisconnectToServer(&dmi_rpc_handle); 
		return (0); 
	}
	usage();
}
