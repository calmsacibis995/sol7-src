// Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)miapi.cc	1.10 96/09/11 Sun Microsystems"

/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */


#include <stdlib.h>
#include "server.h"
//#include <rw/rstream.h>
//#include <string.h>
#include "dmi_error.hh"
#include "miapi.hh"
#include "trace.hh"

bool_t DmiRegister(DmiRegisterIN argin,
				   DmiRegisterOUT *result,
				   DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiRegisterOUT *) NULL) {
		trace("result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE; 
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiRegisterOUT *result_1 = _dmiregister_0x1(&argin, clnt);
	if (result_1 == (DmiRegisterOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiRegisterOUT)); 
	return(TRUE); 
}

bool_t DmiUnregister(DmiUnregisterIN argin,
					 DmiUnregisterOUT *result,
					 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiUnregisterOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;		
		return(FALSE); 
	}
	
	DmiUnregisterOUT *result_1 = _dmiunregister_0x1(&argin, clnt);
	if (result_1 == (DmiUnregisterOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE; 		
		return (FALSE); 
		
	}

	memcpy(result, result_1, sizeof(DmiUnregisterOUT)); 

	return(TRUE); 
}

bool_t DmiGetVersion(DmiGetVersionIN argin,
					 DmiGetVersionOUT *result,
					 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiGetVersionOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	DmiGetVersionOUT *result_1 = _dmigetversion_0x1(&argin, clnt);
	if (result_1 == (DmiGetVersionOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiGetVersionOUT)); 

	return(TRUE); 	
}

bool_t DmiGetConfig(DmiGetConfigIN argin,
					DmiGetConfigOUT *result,
					DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiGetConfigOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	DmiGetConfigOUT *result_1 = _dmigetconfig_0x1(&argin, clnt);
	if (result_1 == (DmiGetConfigOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiGetConfigOUT)); 

	return(TRUE);
}

bool_t DmiSetConfig(DmiSetConfigIN argin,
					DmiSetConfigOUT  *result,
					DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiSetConfigOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}

	DmiSetConfigOUT *result_1 = _dmisetconfig_0x1(&argin, clnt);
	if (result_1 == (DmiSetConfigOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiSetConfigOUT)); 

	return(TRUE);
}

bool_t DmiListComponents(DmiListComponentsIN argin,
						 DmiListComponentsOUT *result,
						 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListComponentsOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}

	DmiListComponentsOUT *result_1 = _dmilistcomponents_0x1(&argin, clnt);
	if (result_1 == (DmiListComponentsOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListComponentsOUT)); 

	return(TRUE);
}

bool_t DmiListComponentsByClass(DmiListComponentsByClassIN argin,
								DmiListComponentsByClassOUT *result,
								DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListComponentsByClassOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}

	DmiListComponentsByClassOUT *result_1 =
		_dmilistcomponentsbyclass_0x1(&argin, clnt);

	if (result_1 == (DmiListComponentsByClassOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListComponentsByClassOUT)); 

	return(TRUE);
}

bool_t DmiListLanguages(DmiListLanguagesIN argin,
						DmiListLanguagesOUT	*result,
						DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListLanguagesOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiListLanguagesOUT *result_1 =
		_dmilistlanguages_0x1(&argin, clnt);

	if (result_1 == (DmiListLanguagesOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListLanguagesOUT)); 

	return(TRUE);
}

bool_t DmiListClassNames(DmiListClassNamesIN argin,
						 DmiListClassNamesOUT *result,
						 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListClassNamesOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}

	DmiListClassNamesOUT *result_1 =
		_dmilistclassnames_0x1(&argin, clnt);

	if (result_1 == (DmiListClassNamesOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListClassNamesOUT)); 

	return(TRUE);
}

bool_t DmiListGroups(DmiListGroupsIN argin,
					 DmiListGroupsOUT *result,
					 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListGroupsOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiListGroupsOUT *result_1 =
		_dmilistgroups_0x1(&argin, clnt);

	if (result_1 == (DmiListGroupsOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListGroupsOUT)); 

	return(TRUE);
}

bool_t DmiListAttributes(DmiListAttributesIN argin,
						 DmiListAttributesOUT *result,
						 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiListAttributesOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiListAttributesOUT *result_1 =
		_dmilistattributes_0x1(&argin, clnt);

	if (result_1 == (DmiListAttributesOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiListAttributesOUT)); 

	return(TRUE);
}

bool_t DmiAddRow(DmiAddRowIN argin,
				 DmiAddRowOUT *result,
				 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiAddRowOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	DmiAddRowOUT *result_1 =
		_dmiaddrow_0x1(&argin, clnt);

	if (result_1 == (DmiAddRowOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiAddRowOUT)); 

	return(TRUE);
}

bool_t DmiDeleteRow(DmiDeleteRowIN argin,
					DmiDeleteRowOUT *result,
					DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiDeleteRowOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiDeleteRowOUT *result_1 =
		_dmideleterow_0x1(&argin, clnt);

	if (result_1 == (DmiDeleteRowOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiDeleteRowOUT)); 

	return(TRUE);
}

bool_t DmiGetMultiple(DmiGetMultipleIN argin,
					  DmiGetMultipleOUT *result,
					  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiGetMultipleOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}
	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	DmiGetMultipleOUT *result_1 =
		_dmigetmultiple_0x1(&argin, clnt);

	if (result_1 == (DmiGetMultipleOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiGetMultipleOUT)); 

	return(TRUE);
}

bool_t DmiSetMultiple(DmiSetMultipleIN argin,
					  DmiSetMultipleOUT *result,
					  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiSetMultipleOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiSetMultipleOUT *result_1 =
		_dmisetmultiple_0x1(&argin, clnt);

	if (result_1 == (DmiSetMultipleOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiSetMultipleOUT)); 

	return(TRUE);
}

bool_t DmiAddComponent(DmiAddComponentIN argin,
					   DmiAddComponentOUT *result,
					   DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiAddComponentOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiAddComponentOUT *result_1 =
		_dmiaddcomponent_0x1(&argin, clnt);

	if (result_1 == (DmiAddComponentOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiAddComponentOUT)); 

	return(TRUE);
}

bool_t DmiAddLanguage(DmiAddLanguageIN argin,
					  DmiAddLanguageOUT *result,
					  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiAddLanguageOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiAddLanguageOUT *result_1 =
		_dmiaddlanguage_0x1(&argin, clnt);

	if (result_1 == (DmiAddLanguageOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiAddLanguageOUT)); 

	return(TRUE);
}

bool_t DmiAddGroup(DmiAddGroupIN argin,
				   DmiAddGroupOUT *result,
				   DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiAddGroupOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiAddGroupOUT *result_1 =
		_dmiaddgroup_0x1(&argin, clnt);

	if (result_1 == (DmiAddGroupOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiAddGroupOUT)); 

	return(TRUE);
}

bool_t DmiDeleteComponent(DmiDeleteComponentIN argin,
						  DmiDeleteComponentOUT *result,
						  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiDeleteComponentOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiDeleteComponentOUT *result_1 =
		_dmideletecomponent_0x1(&argin, clnt);

	if (result_1 == (DmiDeleteComponentOUT *) NULL) {
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		result->error_status = DMIERR_SP_INACTIVE;
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiDeleteComponentOUT)); 

	return(TRUE);
}

bool_t DmiDeleteLanguage(DmiDeleteLanguageIN argin,
						 DmiDeleteLanguageOUT *result,
						 DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiDeleteLanguageOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiDeleteLanguageOUT *result_1 =
		_dmideletelanguage_0x1(&argin, clnt);

	if (result_1 == (DmiDeleteLanguageOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiDeleteLanguageOUT)); 

	return(TRUE);
}

bool_t DmiDeleteGroup(DmiDeleteGroupIN argin,
					  DmiDeleteGroupOUT *result,
					  DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiDeleteGroupOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiDeleteGroupOUT *result_1 =
		_dmideletegroup_0x1(&argin, clnt);

	if (result_1 == (DmiDeleteGroupOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE; 		
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiDeleteGroupOUT)); 

	return(TRUE);
}

bool_t DmiGetAttribute(DmiGetAttributeIN argin,
						 DmiGetAttributeOUT *result, DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiGetAttributeOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiGetAttributeOUT *result_1 =
		_dmigetattribute_0x1(&argin, clnt);

	if (result_1 == (DmiGetAttributeOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE;
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiGetAttributeOUT)); 

	return(TRUE);
}

bool_t DmiSetAttribute(DmiSetAttributeIN argin,
					   DmiSetAttributeOUT *result,
					   DmiRpcHandle *dmi_rpc_handle)
{
	if (result == (DmiSetAttributeOUT *) NULL) {
		trace( "result parameter is null\n");
		result->error_status = DMIERR_ILLEGAL_PARAMETER; 
		return(FALSE);
	}

	if (dmi_rpc_handle == NULL) {
		trace( "dmi rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return (FALSE);
	}
	CLIENT *clnt = (CLIENT *)dmi_rpc_handle->RpcHandle_u.handle; 
	if (clnt == (CLIENT *)NULL) {
		trace( "rpc handle is null\n");
		result->error_status = DMIERR_ILLEGAL_RPC_HANDLE;
		return(FALSE); 
	}
	
	DmiSetAttributeOUT *result_1 =
		_dmisetattribute_0x1(&argin, clnt);

	if (result_1 == (DmiSetAttributeOUT *) NULL) {
		result->error_status = DMIERR_SP_INACTIVE; 
		trace("SP may be inactive.\n"); 
		clnt_perror(clnt, "call failed");
		return (FALSE); 
	}

	memcpy(result, result_1, sizeof(DmiSetAttributeOUT)); 

	return(TRUE);
}
