#ifndef _REGCI_HH
#define _REGCI_HH

/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)regci.hh	1.4 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	


extern bool_t
dmiregisterci(DmiRegisterCiIN *argp, DmiRegisterCiOUT *result); 

extern bool_t 
dmiunregisterci(DmiUnregisterCiIN *argp, DmiUnregisterCiOUT *result); 

extern bool_t
dmioriginateevent(DmiOriginateEventIN *argp, DmiOriginateEventOUT *result); 

#ifdef __cplusplus
}
#endif 	
#endif
