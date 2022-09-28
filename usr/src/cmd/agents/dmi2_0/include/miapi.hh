
#ifndef _MIAPI_HH
#define _MIAPI_HH
/* Copyright 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)miapi.hh	1.8 96/07/23 Sun Microsystems"

#include "api.hh"

#ifdef __cplusplus
extern "C" {
#endif 	

extern bool_t
DmiRegister(DmiRegisterIN argin,
			DmiRegisterOUT *result,
			DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiUnregister(DmiUnregisterIN argin,
			  DmiUnregisterOUT *result,
			  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiGetVersion(DmiGetVersionIN argin,
			  DmiGetVersionOUT *result,
			  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiGetConfig(DmiGetConfigIN argin,
			 DmiGetConfigOUT  *result,
			 DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiSetConfig(DmiSetConfigIN argin,
			 DmiSetConfigOUT  *result,
			 DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiListComponents(DmiListComponentsIN argin,
				  DmiListComponentsOUT *result,
				  DmiRpcHandle *dmi_rpc_handle);

extern bool_t
DmiListComponentsByClass(DmiListComponentsByClassIN argin,
						 DmiListComponentsByClassOUT *result,
						 DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiListLanguages(DmiListLanguagesIN argin,
				 DmiListLanguagesOUT	*result,
				 DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiListClassNames(DmiListClassNamesIN argin,
				  DmiListClassNamesOUT *result,
				  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiListGroups(DmiListGroupsIN argin,
			  DmiListGroupsOUT *result,
			  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiListAttributes(DmiListAttributesIN argin,
				  DmiListAttributesOUT *result,
				  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiAddRow(DmiAddRowIN argin,
		  DmiAddRowOUT *result,
		  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiDeleteRow(DmiDeleteRowIN argin,
			 DmiDeleteRowOUT *result,
			 DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiGetMultiple(DmiGetMultipleIN argin,
			   DmiGetMultipleOUT *result,
			   DmiRpcHandle *dmi_rpc_handle); 
 
extern bool_t
DmiSetMultiple(DmiSetMultipleIN argin,
			   DmiSetMultipleOUT *result,
			   DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiAddComponent(DmiAddComponentIN argin,
				DmiAddComponentOUT *result,
				DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiAddLanguage(DmiAddLanguageIN argin,
			   DmiAddLanguageOUT *result,
			   DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiAddGroup(DmiAddGroupIN argin,
			DmiAddGroupOUT *result,
			DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiDeleteComponent(DmiDeleteComponentIN argin,
				   DmiDeleteComponentOUT *result,
				   DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiDeleteLanguage(DmiDeleteLanguageIN argin,
				  DmiDeleteLanguageOUT *result,
				  DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiDeleteGroup(DmiDeleteGroupIN argin,
			   DmiDeleteGroupOUT *result,
			   DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiGetAttribute(DmiGetAttributeIN argin,
				DmiGetAttributeOUT *result,
				DmiRpcHandle *dmi_rpc_handle); 

extern bool_t
DmiSetAttribute(DmiSetAttributeIN argin,
				DmiSetAttributeOUT *result,
				DmiRpcHandle *dmi_rpc_handle); 

#ifdef __cplusplus
}
#endif 	
#endif
