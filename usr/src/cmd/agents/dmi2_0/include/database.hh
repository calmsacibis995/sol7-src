#ifndef _DATABASE_HH
#define _DATABASE_HH

/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)database.hh	1.4 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	


extern bool_t
dmiaddcomponent(DmiAddComponentIN *argp, DmiAddComponentOUT *result); 

extern bool_t
dmiaddgroup(DmiAddGroupIN *argp, DmiAddGroupOUT *result);

extern bool_t
dmiaddlanguage(DmiAddLanguageIN *argp, DmiAddLanguageOUT *result); 

extern bool_t
dmideletecomponent(DmiDeleteComponentIN *argp, DmiDeleteComponentOUT *result);

extern bool_t
dmideletegroup(DmiDeleteGroupIN *argp, DmiDeleteGroupOUT *result); 

extern bool_t
dmideletelanguage(DmiDeleteLanguageIN *argp, DmiDeleteLanguageOUT *result); 

#ifdef __cplusplus
}
#endif 	

#endif

