/* Copyright 09/16/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)error.h	1.6 96/09/16 Sun Microsystems"



#ifndef _ERROR_H_
#define _ERROR_H_


/***** GLOBAL CONSTANTS *****/

#define DEFAULT_ERROR_SIZE	10000


/***** GLOBAL VARIABLES *****/

extern int error_size;

extern char error_label[];


/***** GLOBAL FUNCTIONS *****/

extern void error_init(char *name, void end());
extern void error_open(char *filename);
extern void error_close_stderr();

extern void error();
extern void error_exit();

extern char *errno_string();
extern char *h_errno_string();

#endif

