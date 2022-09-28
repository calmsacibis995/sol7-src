// Copyright 09/25/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)ci_main.cc	1.13 96/09/25 Sun Microsystems"


#include <stdlib.h>
#include "server.h"
#include <rw/rstream.h>
#include <string.h>
#include "ci_callback.h"
#include "ci_callback_svc.hh"
#include "ciapi.hh"
#include "api.hh"
#include "dmi_error.hh"
#include "util.hh"
#include "trace.hh"
#include <thread.h>


main(int argc, char *argv[])
{

	trace_on(); 
	// This is a must step
	u_long 	prognum = reg_ci_callback();
	cout << "callback prog num " << prognum<< endl;


	ConnectIN connectin;
	connectin.servertype = CISERVER;
	connectin.rpctype = ONC;
	connectin.host = "localhost";
	connectin.nettype = "netpath";
	
	if (argc < 2) {
		cout << "connect to local host" << endl;
		connectin.host = "localhost"; 
	}
	else {
		cout << "connect to " << argv[1] << endl;
		connectin.host = argv[1]; 
	}

	DmiRpcHandle dmi_rpc_handle;

	// must
	if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE)
		exit(-1);

	// we use one thread to deal with rpc call back,
	// and one thread to do the normal job. 
	thread_t tid;
	int error = thr_create(NULL, 0, start_svc_run_thread, NULL, 0, &tid);
	if (error != 0) {
		printf("svc_run thread  create error!\n");
		exit(0); 
	}


	int type = 1;
	int i; 
	DmiHandle_t ci_handle = 0; 
	while (type != 0) {
		cout<< " \n\nPlease select the option, and type return" <<endl;
		cout<< "            0: exit " << endl;
		cout<< "            1: ci reg " << endl;
		cout<< "            2: ci unreg" << endl;
		cout<< "            3: dmioriginateevent " << endl;
		cin >> type; 
		switch (type) {
			case 0:
				DisconnectToServer(&dmi_rpc_handle); 
				exit(0); 
			case 1: 
				DmiRegisterCiIN regciIn;
				DmiRegisterCiOUT regciOut;
				DmiRegisterInfo reginfo; 
				regciIn.regInfo = &reginfo;

				cout<< " \n\nPlease input comp ID" <<endl;
				cin >> reginfo.compId;

				DmiAccessDataList_t accessdata;
				cout<< " \n\nPlease input the number of access data" <<endl;
				cin >> accessdata.list.list_len;
				
				accessdata.list.list_val = (DmiAccessData_t *)
					malloc(sizeof(DmiAccessData_t)*accessdata.list.list_len);
				for ( i= 0;  i< accessdata.list.list_len; i++) {
					cout<< " \n\nPlease input" << i+1 << "th access data: group ID" <<endl;
					cin >> accessdata.list.list_val[i].groupId;
					cout<< " Please input " << i+1<< "th access data:: attr ID" <<endl;
					cin >> accessdata.list.list_val[i].attributeId;
				}

				reginfo.prognum = prognum;
				reginfo.accessData = &accessdata; 
				DmiRegisterCi(regciIn, &regciOut, &dmi_rpc_handle);
				dmi_error(regciOut.error_status);
				if (regciOut.error_status == DMIERR_NO_ERROR) {
					printDmiString(regciOut.dmiSpecLevel);
					cout << *(regciOut.handle) << endl;
					ci_handle = *(regciOut.handle); 
				}
				break;
			case 2:
				DmiUnregisterCiIN unregciIn;
				DmiUnregisterCiOUT unregciOut;
				cout<< " \n\nPlease input comp ID" <<endl;
				cin >> unregciIn.handle;
//				unregciIn.handle = ci_handle; 
				DmiUnregisterCi(unregciIn, &unregciOut, &dmi_rpc_handle);
				dmi_error(unregciOut.error_status);
				break;
			case 3:
				DmiOriginateEventIN eveIn;
				DmiOriginateEventOUT eveOut;
				DmiRowData_t        rowdat;
				DmiAttributeData_t  attrdat[8];
				DmiAttributeValues_t attrval;
				DmiString_t         classstr;

				classstr.body.body_len =10;
				classstr.body.body_val ="TEST CLASS";
 
				attrval.list.list_len = 8;
				attrval.list.list_val = attrdat;

				cout<< " \n\nPlease input comp ID" <<endl;
				cin >>eveIn.compId;


				rowdat.compId = eveIn.compId;
				rowdat.groupId = 2;  /*test value */
				rowdat.className = NULL;
				rowdat.keyList = NULL;
				rowdat.values = &attrval;
                
				attrdat[0].id =  1;
				attrdat[0].data.type = MIF_INTEGER;
				attrdat[0].data.DmiDataUnion_u.integer = 1; 


				attrdat[1].id =  2;
				attrdat[1].data.type = MIF_INTEGER;
				attrdat[1].data.DmiDataUnion_u.integer = 1; 

				attrdat[2].id =  3;
				attrdat[2].data.type = MIF_INTEGER;
				attrdat[2].data.DmiDataUnion_u.integer = 0; 

				attrdat[3].id =  4;
				attrdat[3].data.type = MIF_INTEGER;
				attrdat[3].data.DmiDataUnion_u.integer = 1; 

				attrdat[4].id =  5;
				attrdat[4].data.type = MIF_DISPLAYSTRING;
				attrdat[4].data.DmiDataUnion_u.str = &classstr; 

				attrdat[5].id =  6;
				attrdat[5].data.type = MIF_INTEGER;
				attrdat[5].data.DmiDataUnion_u.integer = 1; 

				attrdat[6].id =  7;
				attrdat[6].data.type = MIF_INTEGER;
				attrdat[6].data.DmiDataUnion_u.integer = 0; 

				attrdat[7].id =  8;
				attrdat[7].data.type = MIF_INTEGER;
				attrdat[7].data.DmiDataUnion_u.integer = 0; 

				eveIn.language = NULL;
				eveIn.timestamp = NULL;
				eveIn.rowData = &rowdat;

				DmiOriginateEvent(eveIn, &eveOut, &dmi_rpc_handle);
				dmi_error(eveOut.error_status);
			default:
				break;
		}
	}

	thr_join(tid, NULL, NULL); 
}

