
#ifndef _CIAPI_HH
#define _CIAPI_HH
/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)ciapi.hh	1.6 96/10/07 Sun Microsystems"

#include "api.hh"
#ifdef __cplusplus
extern "C" {
#endif 	

extern bool_t DmiRegisterCi(DmiRegisterCiIN argin,
							   DmiRegisterCiOUT *result,
							   DmiRpcHandle *dmi_rpc_handle); 

extern bool_t DmiUnregisterCi(DmiUnregisterCiIN argin,
								 DmiUnregisterCiOUT *result,
								 DmiRpcHandle *dmi_rpc_handle);

extern bool_t DmiOriginateEvent(DmiOriginateEventIN argin,
								DmiOriginateEventOUT *result,
								DmiRpcHandle *dmi_rpc_handle); 

#ifdef __cplusplus
}
#endif 	

#endif
