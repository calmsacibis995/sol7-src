// Copyright 10/09/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dmiget.cc	1.6 96/10/09 Sun Microsystems"

#include <stdio.h>
#include <stream.h>
#include <string.h>
#include <stdlib.h>
#include "server.h"
#include "miapi.hh"
#include "util.hh"
#include "dmi_error.hh"
#include "trace.hh"

static int printAttributeValues(DmiAttributeValues_t *values, int attrId)
{
	if ((values== NULL) || (values->list.list_len== 0))
		return (0);
	if (attrId == 0) {
		for (int i= 0; i< values->list.list_len; i++) {
			trace("\nId: %d, \t", values->list.list_val[i].id);
			printDmiDataUnion(&(values->list.list_val[i].data));
		}
	}
	else {
		for (int i= 0; i< values->list.list_len; i++) {
			if (attrId == values->list.list_val[i].id) {
				trace("\nId: %d, \t", values->list.list_val[i].id);
				printDmiDataUnion(&(values->list.list_val[i].data));
				i = values->list.list_len; 
			}
		}
		if (i != values->list.list_len +1) {
			return (-1);
		}
	}

	return (0); 
}

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

    fprintf(stderr, "dmiget [-s hostname] ");
	fprintf(stderr, "-h|{ -c compId [-g groupId] [-a attrId] }\n"); 
	exit(0); 
}

void getTable(int compId, int groupId, int attrId, DmiRpcHandle *dmi_rpc_handle)
{
	if ((compId <= 0) ||
		(groupId <= 0)) return ;
	
	DmiListGroupsOUT listgroupOut;
	DmiListGroupsIN listgroupIn;
	listgroupIn.compId = compId;
	listgroupIn.groupId = groupId;
	listgroupIn.handle = 0;
	listgroupIn.maxCount = 1;
	listgroupIn.getPragma = 1;
	listgroupIn.getDescription = 1;
	listgroupIn.requestMode = DMI_UNIQUE;

	DmiListGroups(listgroupIn, &listgroupOut, dmi_rpc_handle); 
	if (listgroupOut.error_status != DMIERR_NO_ERROR) {
		trace("list groups of comp %d fail with error = ", compId);
		fflush(0);
		dmi_error(listgroupOut.error_status);
		return; 
//		return (0); 
	}

	if ((listgroupOut.reply == NULL) ||
		( listgroupOut.reply->list.list_len == 0)) {
		return ;
	}
	trace("For group %d of component %d:\n", groupId, compId); 
	// get the first row
	DmiGetMultipleOUT getmultiOut;
	DmiGetMultipleIN getmultiIn;
	DmiRowRequest_t rowreq[1]; 


	rowreq[0].requestMode = DMI_UNIQUE;
	rowreq[0].keyList = NULL; // &keyList;
	rowreq[0].compId = compId;
	rowreq[0].groupId = groupId;
	rowreq[0].ids = NULL;

	getmultiIn.handle = 0;
	getmultiIn.request = (DmiMultiRowRequest_t *)
		malloc(sizeof(DmiMultiRowRequest_t)); 
	getmultiIn.request->list.list_len = 1;
	getmultiIn.request->list.list_val = rowreq;

	DmiGetMultiple(getmultiIn, &getmultiOut, dmi_rpc_handle);
	if ((getmultiOut.error_status != DMIERR_NO_ERROR)&&
		(getmultiOut.error_status == DMIERR_UNKNOWN_CI_REGISTRY)){
			trace("Can not get data from component code\n");
//			fflush (0);
//			dmi_error(getmultiOut.error_status); 
		return; 
	}

	if (( getmultiOut.rowData == NULL)||
		(getmultiOut.rowData->list.list_val[0].values== NULL)) {
		trace("no data\n"); 
		return ;
	}
	
	DmiAttributeValues_t *values = getmultiOut.rowData->list.list_val[0].values;
//		trace("First row data\n"); 
	if (printAttributeValues(values, attrId)) {
		trace("Attr %d in group %d of comp %d not exist\n\n",
			  attrId, groupId, compId);
//		return(0);
		return; 
	}
	trace("\n");
			
	DmiAttributeIds_t *keyList = listgroupOut.reply->list.list_val[0].keyList;
	DmiAttributeValues_t *keyValueList;
	if ((keyList == NULL) || (keyList->list.list_len == 0)) {
		keyValueList = NULL;
	}
	else {
		keyValueList = newDmiAttributeValues(keyList->list.list_len);
		for (int i = 0; i < keyList->list.list_len; i++) {
			keyValueList->list.list_val[i].id = keyList->list.list_val[i];
			for (int j = 0; j< values->list.list_len; j++) {
				if (values->list.list_val[j].id == keyList->list.list_val[i]) {
					keyValueList->list.list_val[i].data = values->list.list_val[j].data;
					j = values->list.list_len +1; 
				}
			}
		}
	}
	rowreq[0].keyList = keyValueList; // &keyList;		

	while (1) {
		rowreq[0].requestMode = DMI_NEXT;
		DmiGetMultiple(getmultiIn, &getmultiOut, dmi_rpc_handle);
		if (getmultiOut.error_status != DMIERR_NO_ERROR) {
//				trace("get next row fail with error = \n");
//				fflush (0);
//				dmi_error(getmultiOut.error_status);
			trace("\n"); 
//			return (0);
			return; 
		}

		if (( getmultiOut.rowData == NULL)||
			(getmultiOut.rowData->list.list_val[0].values== NULL)) {
			trace("no data\n"); 
			return;
		}

		values = getmultiOut.rowData->list.list_val[0].values;
//			trace("next row data\n"); 
		if (printAttributeValues(values, attrId)) {
			trace("Attr %d in group %d of comp %d not exist\n\n",
				  attrId, groupId, compId);
//			return(0);
			return; 
		}
		trace("\n"); 

		if (keyValueList != NULL) {
			for (int i = 0; i < keyValueList->list.list_len; i++) {
				for (int j = 0; j< values->list.list_len; j++) {
					if (values->list.list_val[j].id == keyList->list.list_val[i]) {
						keyValueList->list.list_val[i].data = values->list.list_val[j].data;
						j = values->list.list_len +1; 
					}
				}
			}
		}
		else {
			return; 
		}
	}
}

main(int argc, char *argv[])
{
    extern char *optarg;
	extern int optind;
	int opt;
	int errflag = 0;

	bool_t apierr; 
	int compId = 0;
	int groupId = 0;
	int attrId = 0;
	char *hostname = NULL;

	optind = 1;

	if (argc == 1)
		usage();

	
	
    while ((opt = getopt(argc, argv, "hs:c:g:a:")) != EOF) {
		switch (opt) {
			case 'h':
				usage(); 
				break;
			case 's':
				hostname = strdup(optarg);
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
			case '?':		/* usage help */
				errflag++; 
				break; 
		}  /* switch */
	}/* while */

	
	if (errflag)
		usage();
	
	if (optind != argc)
		usage();

	if (compId == 0)
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


	trace("Connecting to dmispd on the %s...\n\n", connectin.host); 
	if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) {
		trace("Fail to connect to %s\n", connectin.host); 
		exit(-1);
	}

	if (groupId > 0) { // get table of particular group
		getTable(compId, groupId, attrId, &dmi_rpc_handle);
	}
	else { // get tables of whole groups in a component
		DmiListGroupsOUT listgOut;
		DmiListGroupsIN listgIn;
		listgIn.compId = compId;
		listgIn.groupId = 0;
		listgIn.handle = 0;
		listgIn.maxCount = 0;
		listgIn.getPragma = 1;
		listgIn.getDescription = 1;
		listgIn.requestMode = DMI_UNIQUE;

		DmiListGroups(listgIn, &listgOut, &dmi_rpc_handle); 
		if (listgOut.error_status != DMIERR_NO_ERROR) {
			trace("list groups of comp %d fail with error = ", compId);
			fflush(0);
			dmi_error(listgOut.error_status);
			return(0); 
		}

		if ((listgOut.reply == NULL) ||
			( listgOut.reply->list.list_len == 0)) {
			trace("no tables to list for component %d\n", compId);
			return(0); 
		}
		for ( int i = 0; i < listgOut.reply->list.list_len; i++) {
			getTable(compId, listgOut.reply->list.list_val[i].id, attrId, &dmi_rpc_handle);
			trace("\n"); 
		}
			
	}
	trace("\n");
	DisconnectToServer(&dmi_rpc_handle); 
	return (0); 
}


