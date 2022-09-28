#ifndef _INITIALIZATION_HH
#define _INITIALIZATION_HH


/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)initialization.hh	1.4 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	

extern void InitDmiInfo(char *config_dir); 

extern bool_t
dmiregister(DmiRegisterIN *argp, DmiRegisterOUT *result); 

extern bool_t
dmiunregister(DmiUnregisterIN *argp, DmiUnregisterOUT *result); 

extern bool_t
dmigetversion(DmiGetVersionIN *argp, DmiGetVersionOUT *result); 

extern bool_t
dmigetconfig(DmiGetConfigIN *argp, DmiGetConfigOUT *result); 

extern bool_t 
dmisetconfig(DmiSetConfigIN *argp, DmiSetConfigOUT *result); 

#ifdef __cplusplus
}
#endif 	

#endif

