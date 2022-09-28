#ifndef _SPCALLCI_HH
#define _SPCALLCI_HH

/* Copyright 08/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)spcallci.hh	1.5 96/08/02 Sun Microsystems"


extern bool_t 
cigetattribute(u_long prognum, CiGetAttributeIN *argp, CiGetAttributeOUT *result); 

extern bool_t 
cigetnextattribute(u_long prognum, CiGetNextAttributeIN *argp, CiGetNextAttributeOUT *result); 

extern bool_t
cisetattribute(u_long prognum, CiSetAttributeIN *argp, DmiErrorStatus_t *result); 
	
extern bool_t
cireserveattribute(u_long prognum, CiReserveAttributeIN *argp, DmiErrorStatus_t *result); 

extern bool_t
cireleaseattribute(u_long prognum, CiReleaseAttributeIN *argp, DmiErrorStatus_t *result); 

extern bool_t
ciaddrow(u_long prognum, CiAddRowIN *argp, DmiErrorStatus_t *result); 

extern bool_t
cideleterow(u_long prognum, CiDeleteRowIN *argp, DmiErrorStatus_t *result); 
	
#endif
