
/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trace.h	1.3 96/07/01 Sun Microsystems"

#ifndef _TRACE_H_
#define _TRACE_H_


/***** GLOBAL CONSTANTS *****/

#define TRACE_LEVEL_MAX		4

#define TRACE_TRAFFIC		0x1
#define TRACE_PACKET		0x2
#define TRACE_PDU		0x4


/***** GLOBAL VARIABLES *****/

extern int trace_level;		/* 0 ... TRACE_LEVEL_MAX */
extern u_long trace_flags;


/***** GLOBAL FUNCTIONS *****/

extern void trace();

extern int trace_set(int level, char *error_label);
extern void trace_reset();
extern void trace_increment();
extern void trace_decrement();


#endif

