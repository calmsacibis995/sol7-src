// Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)ciapi.cc	1.8 96/10/07 Sun Microsystems"


#include <stdlib.h>
#include "server.h"
//#include <rw/rstream.h>
//#include <string.h>
#include "dmi_error.hh"
#include "ciapi.hh"
#include "trace.hh"


bool_t
DmiRegisterCi(DmiRegisterCiIN argin,
			  DmiRegisterCiOUT *result,
			  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiRegisterCiOUT *) NULL) {
		trace("result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace("dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE; 
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace("rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiRegisterCiOUT *tmp_result = _dmiregisterci_0x1(&argin, clnt);
	if (tmp_result == (DmiRegisterCiOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n");
		clnt_perror(clnt, "call failed");
		return(FALSE); 
	}
	memcpy(result, tmp_result, sizeof(DmiRegisterCiOUT)); 
	return(TRUE); 
}


bool_t
DmiUnregisterCi(DmiUnregisterCiIN argin,
				DmiUnregisterCiOUT *result,
				DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiUnregisterCiOUT *) NULL) {
		trace("result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace("dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE; 
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace("rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	DmiUnregisterCiOUT *tmp_result = _dmiunregisterci_0x1(&argin, clnt);
	if (tmp_result == (DmiUnregisterCiOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n");
		clnt_perror(clnt, "call failed");
		return(FALSE); 
	}
	memcpy(result, tmp_result, sizeof(DmiUnregisterCiOUT)); 
	return(TRUE); 
}

bool_t
DmiOriginateEvent(DmiOriginateEventIN argin,
				  DmiOriginateEventOUT *result,
				  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiOriginateEventOUT *) NULL) {
		trace("result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace("dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE; 
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace("rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiOriginateEventOUT  *tmp_result = _dmioriginateevent_0x1(&argin, clnt);
	if (tmp_result == (DmiOriginateEventOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n");
		clnt_perror(clnt, "call failed");
		return(FALSE); 
	}
	memcpy(result, tmp_result, sizeof(DmiOriginateEventOUT)); 
	return(TRUE); 
}

