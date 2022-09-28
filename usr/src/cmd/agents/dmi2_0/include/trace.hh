#ifndef _TRACE_HH
#define _TRACE_HH

/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trace.hh	1.2 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	

extern void error(char *, ...); 
extern void trace(char *, ...); 
extern void trace_on(); 
extern void trace_off(); 
#ifdef __cplusplus
}
#endif 	
#endif
