#ifndef _OPERATION_HH
#define _OPERATION_HH
/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)operation.hh	1.4 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	

extern bool_t
dmigetattribute(DmiGetAttributeIN *argp, DmiGetAttributeOUT *result); 

extern bool_t
dmisetattribute(DmiSetAttributeIN *argp, DmiSetAttributeOUT *result); 

extern bool_t
dmigetmultiple(DmiGetMultipleIN *argp, DmiGetMultipleOUT *result); 

extern bool_t
dmisetmultiple(DmiSetMultipleIN *argp, DmiSetMultipleOUT *result); 

extern bool_t
dmiaddrow(DmiAddRowIN *argp, DmiAddRowOUT *result); 

extern bool_t
dmideleterow(DmiDeleteRowIN *argp, DmiDeleteRowOUT *result); 

#ifdef __cplusplus
}
#endif 	

#endif

