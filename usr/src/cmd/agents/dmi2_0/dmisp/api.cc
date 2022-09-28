// Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)api.cc	1.5 96/09/11 Sun Microsystems"

#include <stdlib.h>
#include "server.h"
//#include <rw/rstream.h>
#include <string.h>
#include "api.hh"
#include "trace.hh"

bool_t ConnectToServer(ConnectIN *argp, DmiRpcHandle *dmi_rpc_handle)
{

	if (argp == NULL) {
		trace(" the in parameter is null!\n"); 
		return (FALSE);
	}
	switch (argp->rpctype) {
		case DCE:
			trace("DCE RPC is not supported in this release.\n"); 
			return (FALSE);
		default:
			trace("Not a  valid rpc type.\n "); 
			return (FALSE);
		case ONC:
			unsigned long prog_num, version_num;
			switch (argp->servertype) {
				case MISERVER:
					prog_num = DMI2_SERVER;
					version_num = DMI2_SERVER_VERSION;
					break;
				case CISERVER:
					prog_num = DMI2_CSERVER;
					version_num = DMI2_CSERVER_VERSION;
					break;
				default:
					trace("Not a valid Server Type\n");
					exit(1);
			}

			CLIENT *thisRpcHandle; 
			if (argp->host == NULL) {
				thisRpcHandle = clnt_create("localhost",
													prog_num,
													version_num,
													argp->nettype);
			}
			else {
				thisRpcHandle = clnt_create(argp->host,
													prog_num,
													version_num,
													argp->nettype);
			}
			if (thisRpcHandle == (CLIENT *) NULL) {
				clnt_pcreateerror(argp->host);
				return (FALSE);
			}
			dmi_rpc_handle->type = ONC;
			dmi_rpc_handle->RpcHandle_u.handle = thisRpcHandle; 
			return(TRUE); 
	}

}

bool_t DisconnectToServer(DmiRpcHandle *dmi_rpc_handle)
{
	if (dmi_rpc_handle != NULL) {
		if (dmi_rpc_handle->type == DCE) {
			trace("DCE RPC is not supported in this release.\n"); 
			return (FALSE);
		}			
		clnt_destroy(dmi_rpc_handle->RpcHandle_u.handle);
	}
}
