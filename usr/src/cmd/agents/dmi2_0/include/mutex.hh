#ifndef _MUTEX_HH
#define _MUTEX_HH

/* Copyright 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)mutex.hh	1.3 96/07/23 Sun Microsystems"

extern void init_component_lock(); 
extern void acquire_component_lock(); 
extern void release_component_lock(); 

#endif
