/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)signals.h	1.3 96/07/01 Sun Microsystems"

#ifndef _SIGNALS_H_
#define _SIGNALS_H_

/***** GLOBAL FUNCTIONS *****/

extern int signals_init(void signals_sighup(), void signals_exit(), char *error_label);


#endif

