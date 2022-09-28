// Copyright 09/16/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)mi_main.cc	1.29 96/09/16 Sun Microsystems"

#include <stdlib.h>
#include "server.h"
#include <rw/rstream.h>
#include <string.h>
#include <ctype.h>

#include "miapi.hh"
#include "util.hh"
#include "dmi_error.hh"
static int is_number (char *buf, int low, int up)
{
	int len, i;

	if (buf == NULL)
		return (-1);
	
	len = strlen(buf);
	for (i= 0; i < len; i++) 
		if (!isdigit(buf[i])) {
			fprintf(stderr, "\n%s is not a valid none negative number!\n\n", buf);
			return(-1);
		}
	int level = atoi(buf);

	if (up > low) {
		if (level > up || level < low) {
			fprintf(stderr, "\n%s is not a valid choice!\n\n", buf);
			return (-1);
		}
	}
	else {
		if (level < low) {
			fprintf(stderr, "\n%s is not a valid choice!\n\n", buf);
			return (-1);
		}
	}		
	return (0);
}

main(int argc, char *argv[])
{
	ConnectIN connectin;
	connectin.servertype = MISERVER;
	connectin.rpctype = ONC;
	connectin.host = "localhost";
	connectin.nettype = "netpath";

	int i;
	
	if (argc < 2) {
		cout << "connect to local host" << endl;
		connectin.host = "localhost"; 
	}
	else {
		cout << "connect to " << argv[1] << endl;
		connectin.host = argv[1]; 
	}

	DmiRpcHandle dmi_rpc_handle;
	if (ConnectToServer(&connectin, &dmi_rpc_handle) == FALSE) exit(-1);




	int type = 1;
	while (type != 0) {
		cout<< " \n\nPlease select following the option, and type return" <<endl;
		cout<< "            0: exit " << endl;
		cout<< "            1: add comp " << endl;
		cout<< "            2: get version" << endl;
		cout<< "            3: add group " << endl;
		cout<< "            4: list comps " << endl;
		cout<< "            5: delete comp " << endl;
		cout<< "            6: delete group " << endl;
		cout<< "            7: list groups " << endl;
		cout<< "            8: list attrs " << endl; 
		cout<< "            9: list classnames " << endl;
		cout<< "            10: get config " << endl;
		cout<< "            11: set config " << endl;
		cout<< "            12: add language " << endl;
		cout<< "            13: list language " << endl;		
		cout<< "            14: del language " << endl;		
		cout<< "            15: get attribute " << endl;		
		cout<< "            16: get multiple " << endl;		
		cout<< "            17: set attr " << endl;		
		cout<< "            18: set multiple " << endl;		
		cout<< "            19: list by class " << endl;
		cout<< "            20: Add row"        << endl; 
		cout<< "            21: Delete row"        << endl; 
//		cin >> type;
		char inbuf[240]; 
		scanf("%s", inbuf);
		if (is_number(inbuf, 0, 21)) continue; 
		type = atoi(inbuf); 

		switch (type) {
			case 0:
				DisconnectToServer(&dmi_rpc_handle); 
				exit(0); 
			case 1:
				DmiAddComponentIN addcompIn;
				DmiAddComponentOUT result;
				DmiFileDataList_t filedatalist;
				filedatalist.list.list_len = 1;
				filedatalist.list.list_val = (DmiFileDataInfo_t *)
					malloc(sizeof(DmiFileDataInfo_t)*
						   filedatalist.list.list_len);
				char filename[64];
				for (i = 0; i< filedatalist.list.list_len; i++) {
					cout << "please input the " << i+1
						 << "th mif file name (MAX LENGTH = 64)" << endl;
					cin >> filename; 
					filedatalist.list.list_val[i].fileType = DMI_MIF_FILE_NAME;
					filedatalist.list.list_val[i].fileData =
						newDmiOctetStringFromString(filename); 
				}
				addcompIn.handle = 0;
				addcompIn.fileData = &filedatalist;
	
				if (DmiAddComponent(addcompIn,&result,
									&dmi_rpc_handle) == TRUE) {
					dmi_error(result.error_status);
					cout << "compId " << result.compId << endl;
				}

				break;
			case 2:
				DmiGetVersionIN getversionIn;
				DmiGetVersionOUT getversionOut;

				getversionIn.handle = 0;

				if (DmiGetVersion(getversionIn,
								  &getversionOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(getversionOut.error_status);
					printDmiString(getversionOut.dmiSpecLevel);
					printDmiString(getversionOut.description);
				}
				break;
			case 3:
				DmiAddGroupIN addgroupIn;
				DmiAddGroupOUT addgroupOut;
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				addgroupIn.compId = atoi(inbuf); 

//				cin >> addgroupIn.compId;

				filedatalist.list.list_len = 2;
				filedatalist.list.list_val = (DmiFileDataInfo_t *)
					malloc(sizeof(DmiFileDataInfo_t)*
						   filedatalist.list.list_len);
				for (i = 0; i< filedatalist.list.list_len; i++) {
					cout << "please input the " << i+1
						 << "th group schema name (MAX LENGTH = 64)" << endl;
					cin >> filename; 
					filedatalist.list.list_val[i].fileType = DMI_MIF_FILE_NAME;
					filedatalist.list.list_val[i].fileData =
						newDmiOctetStringFromString(filename); 
				}
				addgroupIn.handle = 0;
				addgroupIn.fileData = &filedatalist;
				if (DmiAddGroup(addgroupIn, &addgroupOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(addgroupOut.error_status);
					cout << "group " << addgroupOut.groupId
						 << " added" << endl; 
				}
				break;
			case 4:

				DmiListComponentsIN listcompIn; 
				DmiListComponentsOUT listcompOut; 
				listcompIn.handle = 0;
				int reqmode; 
				cout << "Please select a request mode (1-DMI_UNIQUE, 2-DMI_FIRST, 3-DMI_NEXT)"
					 << endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 1, 3)) continue;
				reqmode = atoi(inbuf); 
//				cin >> reqmode;
				switch (reqmode) {
					case 1:
						listcompIn.requestMode = DMI_UNIQUE;
						cout << "Please input comp ID" <<endl;
						scanf("%s", inbuf);
						if (is_number(inbuf, 0, 0)) continue;
						listcompIn.compId = atoi(inbuf); 

//						cin >> listcompIn.compId;
						break; 
					case 2:
						listcompIn.requestMode = DMI_FIRST;
						listcompIn.compId = 0; 
						break; 
					case 3:
						cout << "Please input comp ID" <<endl;
						scanf("%s", inbuf);
						if (is_number(inbuf, 0, 0)) continue;
						listcompIn.compId = atoi(inbuf); 
//						cin >> listcompIn.compId;
						listcompIn.requestMode = DMI_NEXT;
						break; 
					default:
						cout << "invalid request mode, use default DMI_UNIQUE" << endl;
						listcompIn.requestMode = DMI_UNIQUE;
						cout << "Please input comp ID" <<endl;
						scanf("%s", inbuf);
						if (is_number(inbuf, 0, 0)) continue;
						listcompIn.compId = atoi(inbuf); 
//						cin >> listcompIn.compId;
						break;
				}
				cout << "Input the max count to list, 0 -- list all" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listcompIn.maxCount = atoi(inbuf); 
//				cin >> listcompIn.maxCount;
				listcompIn.getPragma = 0; 
				listcompIn.getDescription = 0;

				
				if (DmiListComponents(listcompIn, &listcompOut,
									  &dmi_rpc_handle) == TRUE) {
					dmi_error(listcompOut.error_status);
					printCompList(listcompOut.reply); 
				}
				break;
			case 5:
				DmiDeleteComponentOUT delcompOut;
				DmiDeleteComponentIN delcompIn;
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				delcompIn.compId = atoi(inbuf); 
//				cin >> delcompIn.compId;
				delcompIn.handle = 0;
				if (DmiDeleteComponent(delcompIn, &delcompOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(delcompOut.error_status);
				}
				break;
			case 6:
				DmiDeleteGroupOUT delgroupOut;
				DmiDeleteGroupIN delgroupIn;
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				delgroupIn.compId = atoi(inbuf);
				//				cin >> delgroupIn.compId;
				cout<< " \n\nPlease input group ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				delgroupIn.groupId = atoi(inbuf);
				//cin >> delgroupIn.groupId;
				delgroupIn.handle = 0;
				if (DmiDeleteGroup(delgroupIn, &delgroupOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(delgroupOut.error_status);
				}
				break;
			case 7:
				DmiListGroupsOUT listgroupOut;
				DmiListGroupsIN listgroupIn;
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listgroupIn.compId = atoi(inbuf);
//				cin >> listgroupIn.compId;
				cout<< " \n\nPlease input group ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listgroupIn.groupId = atoi(inbuf);
//				cin >> listgroupIn.groupId;
				listgroupIn.handle = 0;
				listgroupIn.requestMode = DMI_UNIQUE;
				listgroupIn.maxCount = 0;
				listgroupIn.getPragma = TRUE;
				listgroupIn.getDescription = TRUE;
				if (DmiListGroups(listgroupIn, &listgroupOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(listgroupOut.error_status);
				    printGroupList(listgroupOut.reply); 
				}
				break; 
			case 8:
				DmiListAttributesIN listattIn;
				DmiListAttributesOUT listattOut;
				listattIn.handle = 0;
				listattIn.requestMode = DMI_UNIQUE;
				listattIn.maxCount = 0;
				listattIn.getPragma = TRUE;
				listattIn.getDescription = TRUE;
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listattIn.compId = atoi(inbuf);
//				cin >> listattIn.compId;
				cout<< " \n\nPlease input group ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listattIn.groupId = atoi(inbuf);
//				cin >> listattIn.groupId;
				listattIn.attribId = 0;
				if (DmiListAttributes(listattIn, &listattOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(listattOut.error_status);
					printAttrList(listattOut.reply); 
				}
				break; 
			case 9:
				DmiListClassNamesOUT namesOut;
				DmiListClassNamesIN namesIn; 
				cout<< " \n\nPlease input comp ID" <<endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				namesIn.compId = atoi(inbuf);
//				cin >> namesIn.compId;
				namesIn.handle = 0;
				namesIn.maxCount = 0;
				if (DmiListClassNames(namesIn, &namesOut, &dmi_rpc_handle)
					== TRUE ) {
					dmi_error(namesOut.error_status);
					printClassNameList(namesOut.reply);
				}
				break;
			case 10:
				DmiGetConfigIN getconfIn;
				DmiGetConfigOUT getconfOut;
				getconfIn.handle = 0;
				if (DmiGetConfig(getconfIn, &getconfOut,  &dmi_rpc_handle)
					== TRUE) {
					dmi_error(getconfOut.error_status);
					printDmiString(getconfOut.language);
				}
				break;
			case 11:
				DmiSetConfigIN setconfIn;
				DmiSetConfigOUT setconfOut;
				setconfIn.handle = 0;
				char str[32];
				cout << "Please input the language (less than 32 chars)"
					 << endl; 
				cin >>str; 
				setconfIn.language = newDmiString(str); 
				if (DmiSetConfig(setconfIn, &setconfOut,  &dmi_rpc_handle)
					== TRUE) {
					dmi_error(getconfOut.error_status);
				}
				freeDmiString(setconfIn.language); 
				break;
			case 12:
				DmiAddLanguageOUT addlangOut;
				DmiAddLanguageIN addlangIn;
				addlangIn.handle = 0;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				addlangIn.compId = atoi(inbuf);
//				cin >> addlangIn.compId;
				addlangIn.fileData = NULL;
				if (DmiAddLanguage(addlangIn, &addlangOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(addlangOut.error_status);
				}
				break;
			case 13:
				DmiListLanguagesIN listlangIn;
				DmiListLanguagesOUT listlangOut;
				listlangIn.handle = 0;
				listlangIn.maxCount = 0;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				listlangIn.compId = atoi(inbuf);
//				cin >> listlangIn.compId;
				if (DmiListLanguages(listlangIn, &listlangOut, &dmi_rpc_handle)
					== TRUE) {
					dmi_error(listlangOut.error_status);
				}
				break;
			case 14:
				DmiDeleteLanguageIN dellangIn;
				DmiDeleteLanguageOUT dellangOut;
				dellangIn.handle = 0;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				dellangIn.compId = atoi(inbuf);
//				cin >> dellangIn.compId;
//				char str[32];
				cout << "Please input the language (less than 32 chars)"
					 << endl; 
				cin >>str; 
				dellangIn.language = newDmiString(str);
				if (DmiDeleteLanguage(dellangIn, &dellangOut,
									  &dmi_rpc_handle) == TRUE) {
					dmi_error(dellangOut.error_status);
				}
				freeDmiString(dellangIn.language);
				break;
			case 15:
				DmiGetAttributeOUT getattOut;
				DmiGetAttributeIN getattIn;
				DmiAttributeValues_t keyList;

				getattIn.handle =0;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				getattIn.compId = atoi(inbuf);
//				cin >> getattIn.compId;
				cout << "Please input the groupId" << endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				getattIn.groupId = atoi(inbuf);
//				cin >> getattIn.groupId;
//				getattIn.groupId = 42; 
				cout << "Please input the attrId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				getattIn.attribId = atoi(inbuf);
//				cin >> getattIn.attribId;

				getattIn.keyList = NULL;
				
				if (getattIn.groupId == 2) {
					keyList.list.list_len = 1;
					keyList.list.list_val = (DmiAttributeData_t *)malloc
						(sizeof (DmiAttributeData_t)*keyList.list.list_len );
					keyList.list.list_val[0].id = 10;
					keyList.list.list_val[0].data.type = MIF_INTEGER;
					cout << " Please input key value" << endl;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					keyList.list.list_val[0].data.DmiDataUnion_u.integer = atoi(inbuf);
					
//					cin >> keyList.list.list_val[0].data.DmiDataUnion_u.integer; 
					
 					getattIn.keyList = &keyList; 
				}
				if (DmiGetAttribute(getattIn, &getattOut,
									&dmi_rpc_handle) == TRUE) {
					dmi_error(getattOut.error_status);
					printDmiDataUnion(getattOut.value); 
				}
				free(keyList.list.list_val);
				break;
			case 16:
				DmiGetMultipleOUT getmultiOut;
				DmiGetMultipleIN getmultiIn;
				DmiRowRequest_t rowreq[2]; 


				rowreq[0].requestMode = DMI_UNIQUE;
//				rowreq[0].requestMode = DMI_NEXT;

				keyList.list.list_len = 1;
				keyList.list.list_val = (DmiAttributeData_t *)malloc
					(sizeof (DmiAttributeData_t)*keyList.list.list_len );
				keyList.list.list_val[0].id = 1;
				keyList.list.list_val[0].data.type = MIF_DISPLAYSTRING;		
				keyList.list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Circus"); 
				rowreq[0].keyList = NULL; // &keyList;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				rowreq[0].compId = atoi(inbuf);
//				cin >> rowreq[0].compId;
//				rowreq[0].compId = 2; 
				cout << "Please input the groupId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				rowreq[0].groupId = atoi(inbuf);
//				cin >> rowreq[0].groupId;
				rowreq[0].ids = NULL; 
/*				rowreq[0].ids = (DmiAttributeIds_t *) malloc(sizeof(DmiAttributeIds_t));
				
				rowreq[0].ids->list.list_len = 2;
				rowreq[0].ids->list.list_val = (DmiId_t *)
					malloc(sizeof (DmiId_t)*rowreq[0].ids->list.list_len);
				for (i = 0; i< rowreq[0].ids->list.list_len; i++) {
					cout << "Please input the attrId" << endl;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					rowreq[0].ids->list.list_val[i] = atoi(inbuf);
//					cin >> rowreq[0].ids->list.list_val[i];
				}
				*/
/*				
				rowreq[1].requestMode = DMI_UNIQUE;
				rowreq[1].keyList = NULL;
				cout << "Please input the compId" << endl; 
				cin >> rowreq[1].compId;
//				rowreq[1].compId = 1;
cout << "Please input the groupId" << endl; 
cin >> rowreq[1].groupId;
rowreq[1].ids = (DmiAttributeIds_t *) malloc(sizeof(DmiAttributeIds_t)); 
rowreq[1].ids->list.list_len = 2;
rowreq[1].ids->list.list_val = (DmiId_t *)
malloc(sizeof (DmiId_t)*rowreq[1].ids->list.list_len);
for (i = 0; i< rowreq[1].ids->list.list_len; i++) {
cout << "Please input the attrId" << endl;
cin >> rowreq[1].ids->list.list_val[i];
}
*/
				getmultiIn.handle = 0;
				getmultiIn.request = (DmiMultiRowRequest_t *)
					malloc(sizeof(DmiMultiRowRequest_t)); 
				getmultiIn.request->list.list_len = 1;
				getmultiIn.request->list.list_val = rowreq;
				if (DmiGetMultiple(getmultiIn, &getmultiOut, &dmi_rpc_handle) ==
					TRUE) {
					dmi_error(getmultiOut.error_status);
					printDmiMultiRowData(getmultiOut.rowData); 
				}
				break;
			case 17:
				DmiSetAttributeOUT setattOut;
				DmiSetAttributeIN setattIn;
				setattIn.handle =0;
				cout << "Please input the compId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				setattIn.compId = atoi(inbuf);
///				cin >> setattIn.compId;
				cout << "Please input the groupId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				setattIn.groupId = atoi(inbuf);
//				cin >> setattIn.groupId;
				cout << "Please input the attrId" << endl; 
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				setattIn.attribId = atoi(inbuf);
//				cin >> setattIn.attribId;
				cout << "Please input the set mode (1, 2, 3)" << endl;
				int setmode ;
				scanf("%s", inbuf);
				if (is_number(inbuf, 1, 3)) continue;
				setmode = atoi(inbuf);
//				cin >> setmode;
				switch (setmode) {
					case 1:
						setattIn.setMode = DMI_SET;
						break; 
					case 2:
						setattIn.setMode = DMI_RESERVE;
						break; 
					case 3:
						setattIn.setMode = DMI_RELEASE;
						break; 
					default:
						cout << "invalid set mode" << endl;
						break;
				}
				DmiDataUnion_t value;
				value.type = MIF_DISPLAYSTRING;
				value.DmiDataUnion_u.str = newDmiString("new string"); 
				
				setattIn.value = &value;  
				setattIn.keyList = NULL;
				if (setattIn.groupId == 2) {
					keyList.list.list_len = 1;
					keyList.list.list_val = (DmiAttributeData_t *)malloc
						(sizeof (DmiAttributeData_t)*keyList.list.list_len );
					keyList.list.list_val[0].id = 10;
					keyList.list.list_val[0].data.type = MIF_INTEGER;
					cout << " Please input key value" << endl;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					keyList.list.list_val[0].data.DmiDataUnion_u.integer = atoi(inbuf);
					
//					cin >> keyList.list.list_val[0].data.DmiDataUnion_u.integer; 
					
 					setattIn.keyList = &keyList; 
				}
				if (DmiSetAttribute(setattIn, &setattOut, &dmi_rpc_handle) ==
					TRUE) {
					dmi_error(setattOut.error_status);
				}
				break;
			case 18:
				DmiSetMultipleIN setmIn;
				DmiSetMultipleOUT setmOut;

				setmIn.handle = 0;
				cout << "Please input the set mode (1, 2, 3)" << endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 1, 3)) continue;
				setmode = atoi(inbuf);
//				cin >> setmode;
				switch (setmode) {
					case 1:
						setmIn.setMode = DMI_SET;
						break; 
					case 2:
						setmIn.setMode = DMI_RESERVE;
						break; 
					case 3:
						setmIn.setMode = DMI_RELEASE;
						break; 
					default:
						cout << "invalid set mode" << endl;
						break;
				}
				setmIn.rowData =
					(DmiMultiRowData_t *)malloc(sizeof(DmiMultiRowData_t));
				setmIn.rowData->list.list_len = 1;
				setmIn.rowData->list.list_val = (DmiRowData_t *)
					malloc(sizeof(DmiRowData_t)*setmIn.rowData->list.list_len);
				
				for (i= 0; i< setmIn.rowData->list.list_len; i++){
					cout << "for %dth row" << i<< endl;
					cout << " component ID: " ;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					setmIn.rowData->list.list_val[i].compId = atoi(inbuf);
//					cin >> setmIn.rowData->list.list_val[i].compId;
					cout << " group ID: " ;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					setmIn.rowData->list.list_val[i].groupId = atoi(inbuf);
//					cin >> setmIn.rowData->list.list_val[i].groupId;
					setmIn.rowData->list.list_val[i].className = NULL;
					setmIn.rowData->list.list_val[i].keyList = NULL;
					setmIn.rowData->list.list_val[i].values =
						(DmiAttributeValues_t *)malloc
						(sizeof(DmiAttributeValues_t));
					setmIn.rowData->list.list_val[i].values->list.list_len = 2;
					setmIn.rowData->list.list_val[i].values->list.list_val =
						(DmiAttributeData_t *)malloc(
							sizeof(DmiAttributeData_t)*
							setmIn.rowData->list.list_val[i].values->list.list_len);
					cout << " attr ID: " ;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					setmIn.rowData->list.list_val[i].values->list.list_val[0].id = atoi(inbuf);
//					cin >> setmIn.rowData->list.list_val[i].values->list.list_val[0].id; 
					setmIn.rowData->list.list_val[i].values->list.list_val[0].data.type  = MIF_DISPLAYSTRING;
					setmIn.rowData->list.list_val[i].values->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("dmi");						
					cout << " attr ID: " ;
					scanf("%s", inbuf);
					if (is_number(inbuf, 0, 0)) continue;
					setmIn.rowData->list.list_val[i].values->list.list_val[1].id = atoi(inbuf);
//					cin >> setmIn.rowData->list.list_val[i].values->list.list_val[1].id; 
					setmIn.rowData->list.list_val[i].values->list.list_val[1].data.type  = MIF_DISPLAYSTRING;
					setmIn.rowData->list.list_val[i].values->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("dmi");						
				}
				if (DmiSetMultiple(setmIn, &setmOut, &dmi_rpc_handle) ==
					TRUE) {
					dmi_error(setmOut.error_status);
				}
				break;
			case 19:
				DmiListComponentsByClassOUT byclassOut;
				DmiListComponentsByClassIN byclassIn; 

				byclassIn.handle = 0;
				byclassIn.requestMode = DMI_UNIQUE;
				byclassIn.maxCount = 0;
				byclassIn.getPragma = 0; 
				byclassIn.getDescription = 0;
				byclassIn.compId = 0;
				byclassIn.className = newDmiString("Group 1001 classname");
				byclassIn.keyList = NULL; 
				if (DmiListComponentsByClass(byclassIn, &byclassOut,
											 &dmi_rpc_handle) == TRUE) {
					dmi_error(byclassOut.error_status);
					printCompList(byclassOut.reply); 
				}
				
				break;
			case 20:
				DmiAddRowIN addrowIn;
				DmiAddRowOUT addrowOut;
				DmiRowData_t rowdata;
				cout << "Comp ID: " << endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				rowdata.compId  = atoi(inbuf);
//				cin >> 	rowdata.compId ;
				cout << "Group ID: " << endl;
				scanf("%s", inbuf);
				if (is_number(inbuf, 0, 0)) continue;
				rowdata.groupId  = atoi(inbuf);
//				cin >>  rowdata.groupId ; 
#if 0
				rowdata.className = newDmiString("DMTF|Software Example|001");
				rowdata.keyList = newDmiAttributeValues(1);
				rowdata.keyList->list.list_val[0].id = 0;
				rowdata.keyList->list.list_val[0].data.type = MIF_DISPLAYSTRING;				
				rowdata.keyList->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Circus");
				rowdata.values = newDmiAttributeValues(2);
				rowdata.values->list.list_val[0].id =  0;
				rowdata.values->list.list_val[0].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("new row");
				rowdata.values->list.list_val[1].id =  2;
				rowdata.values->list.list_val[1].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("new data");
#else
				rowdata.className = newDmiString("DMTF|DevNames|1.0");
				rowdata.keyList = NULL; 
				rowdata.values = newDmiAttributeValues(4);
				rowdata.values->list.list_val[0].id =  10;
				rowdata.values->list.list_val[0].data.type = MIF_INTEGER;				
				rowdata.values->list.list_val[0].data.DmiDataUnion_u.integer = 100; 
				rowdata.values->list.list_val[1].id =  20;
				rowdata.values->list.list_val[1].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("new data");
				rowdata.values->list.list_val[2].id =  30;
				rowdata.values->list.list_val[2].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[2].data.DmiDataUnion_u.str = newDmiString("new data");
				rowdata.values->list.list_val[3].id =  40;
				rowdata.values->list.list_val[3].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[3].data.DmiDataUnion_u.str = newDmiString("new data");
#endif				
				addrowIn.handle = 0;
				addrowIn.rowData = &rowdata;
				if (DmiAddRow(addrowIn, &addrowOut, &dmi_rpc_handle) == TRUE) {
					dmi_error(addrowOut.error_status);
				}
				break;
			case 21:
				DmiDeleteRowIN deleterowIn;
				DmiDeleteRowOUT deleterowOut;
//				DmiRowData_t rowdata;
				rowdata.compId = 2;
#if 0				
				rowdata.groupId = 42;
				rowdata.className = newDmiString("DMTF|Software Example|001");
				rowdata.keyList = newDmiAttributeValues(1);
				rowdata.keyList->list.list_val[0].id = 1;
				rowdata.keyList->list.list_val[0].data.type = MIF_DISPLAYSTRING;				
				rowdata.keyList->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("Circus");
				rowdata.values = newDmiAttributeValues(2);
				rowdata.values->list.list_val[0].id =  0;
				rowdata.values->list.list_val[0].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[0].data.DmiDataUnion_u.str = newDmiString("new row");
				rowdata.values->list.list_val[1].id =  2;
				rowdata.values->list.list_val[1].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("new data");

#else
				rowdata.groupId = 2;
				rowdata.className = newDmiString("DMTF|DevNames|1.0");
				rowdata.keyList = newDmiAttributeValues(1);
				rowdata.keyList->list.list_val[0].id = 10;
				rowdata.keyList->list.list_val[0].data.type = MIF_INTEGER;				
				rowdata.keyList->list.list_val[0].data.DmiDataUnion_u.integer = 10;
				rowdata.values = newDmiAttributeValues(4);
				rowdata.values->list.list_val[0].id =  10;
				rowdata.values->list.list_val[0].data.type = MIF_INTEGER;				
				rowdata.values->list.list_val[0].data.DmiDataUnion_u.integer = 10;
				rowdata.values->list.list_val[1].id =  20;
				rowdata.values->list.list_val[1].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[1].data.DmiDataUnion_u.str = newDmiString("new data");
				rowdata.values->list.list_val[2].id =  30;
				rowdata.values->list.list_val[2].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[2].data.DmiDataUnion_u.str = newDmiString("new data");
				rowdata.values->list.list_val[3].id =  40;
				rowdata.values->list.list_val[3].data.type = MIF_DISPLAYSTRING;				
				rowdata.values->list.list_val[3].data.DmiDataUnion_u.str = newDmiString("new data");
				
#endif				
				deleterowIn.handle = 0;
				deleterowIn.rowData = &rowdata;
				if (DmiDeleteRow(deleterowIn, &deleterowOut, &dmi_rpc_handle) == TRUE) {
					dmi_error(deleterowOut.error_status);
				}
				break;
				
			default:
				type = 1; 
				break;
		}
	}
}
