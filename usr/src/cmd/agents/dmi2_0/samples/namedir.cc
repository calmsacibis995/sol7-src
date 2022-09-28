// Copyright 08/30/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)namedir.cc	1.4 96/08/30 Sun Microsystems"

#include <stdlib.h>
#include "server.h"
#include <string.h>
#include <ctype.h>
#include "ci_callback.h"
#include "ci_callback_svc.hh"
#include "ciapi.hh"
#include "miapi.hh"
#include "api.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "trace.hh"

static void usage()
{
    fprintf(stderr, "\n\ninstall and register namedir.mif in the SP. \n\n");
    fprintf(stderr, "Usage:\n");

    fprintf(stderr, "  namedir\n");
	fprintf(stderr, "  [-t]\t(set trace on)\n");
	fprintf(stderr, "  [-r componentID]\t(registion only, default is installation and registration.)\n");
	fprintf(stderr, "  [-h]\n"); 
	exit(0); 
}
static int is_compId (char *buf)
{
	int len, i;

	if (buf == NULL)
		return (-1); 
	
	len = strlen(buf);
	for (i= 0; i < len; i++) 
		if (!isdigit(buf[i])) {
		fprintf(stderr, "\n%s is not a valid component Id!\n\n", buf);
		return(-1);
		}
	int level = atoi(buf);
	
	if (level < 0) {
		fprintf(stderr, "\n%s is not a valid component Id!\n\n", buf);
		return (1);
	}
	return (level);
}

main(int argc, char *argv[])
{

    extern char *optarg;
	extern int optind;
	int opt;

	optind = 1;

/*	char *hostname = NULL; */
	int compId = 0; 
    while ((opt = getopt(argc, argv, "th-r:")) != EOF) {
		switch (opt) {
			case 't':
				trace_on();
				break;
			case 'h':
				usage();
				break;
/*			case 's':
				hostname = strdup(optarg);
				break; */
			case 'r':
				if ((compId = is_compId(optarg)) < 0)
					usage(); 
				break; 
			case '?':		/* usage help */
				usage();
				break; 
			default:
				usage();
				break;
		}  /* switch */
	}/* while */

	if (optind != argc)
		usage();

	ConnectIN connectin;
	DmiRpcHandle dmi_rpc_handle;

	connectin.rpctype = ONC;
	connectin.nettype = "netpath";
	connectin.host = "localhost";		

	if (compId == 0) {
		// if there is need for install the component, install it first!
		// define RPC binding parameters
		connectin.servertype = MISERVER;

	// first, connect to Management Interface Server,
	// to install the component
		if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE)
			exit(-1);

	// install namedir.mif
		DmiAddComponentIN addcompIn;
		DmiAddComponentOUT addcompOut;
		DmiFileDataList_t filedatalist;
		filedatalist.list.list_len = 1;
		filedatalist.list.list_val = (DmiFileDataInfo_t *)
			malloc(sizeof(DmiFileDataInfo_t)*
				   filedatalist.list.list_len);
		filedatalist.list.list_val[0].fileType = DMI_MIF_FILE_NAME;
		filedatalist.list.list_val[0].fileData =
			newDmiOctetStringFromString("namedir.mif"); 
		addcompIn.handle = 0;
		addcompIn.fileData = &filedatalist;
	
		DmiAddComponent(addcompIn,&addcompOut,&dmi_rpc_handle); 
		if (addcompOut.error_status != DMIERR_NO_ERROR) {
			trace("\n\n"); 
			dmi_error(addcompOut.error_status);
			error("install namedir.mif failed\n");
			exit(1); 
		}
		trace("installed namedir.mif as comp %d\n", addcompOut.compId);
		DisconnectToServer(&dmi_rpc_handle);
		compId = addcompOut.compId; 
	}	

	// second, connect to Component Interface Server,
	// to register the component just installed
	connectin.servertype = CISERVER;
	if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE)
		exit(-1);

	DmiRegisterCiIN regciIn;
	DmiRegisterCiOUT regciOut;
	DmiRegisterInfo reginfo; 
	regciIn.regInfo = &reginfo;

	// we register the same component just installed
	reginfo.compId = compId;

	// we have to define which attribute and group
	// the component code is going to manage.
	// Here, the ci code manages all the attributes in group 2
	DmiAccessDataList_t accessdata;
	accessdata.list.list_len = 4;
	accessdata.list.list_val = (DmiAccessData_t *)
		malloc(sizeof(DmiAccessData_t)*accessdata.list.list_len);
	for (int i= 0;  i< accessdata.list.list_len; i++) {
		accessdata.list.list_val[i].groupId = 2;
		accessdata.list.list_val[i].attributeId = (i+1)*10;
	}
	
	// we have to get the callback function entry point,
	// otherwise, SP is not able to call ci callback functions. 
	reginfo.prognum = reg_ci_callback();
	reginfo.accessData = &accessdata; 
	DmiRegisterCi(regciIn, &regciOut, &dmi_rpc_handle);
	if (regciOut.error_status == DMIERR_NO_ERROR) {
		trace("register comp %d at SP\n", reginfo.compId);
		// wait for the ci callbacks
		svc_run(); 
	}
	else {
		dmi_error(regciOut.error_status);
		error("register comp %d fail\n", reginfo.compId); 
		exit(1);
	}
}

