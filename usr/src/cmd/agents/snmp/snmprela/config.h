/* Copyright 07/08/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)config.h	1.4 96/07/08 Sun Microsystems"

/*
 * HISTORY
 * 5-13-96      Jerry Yeung     support security file
 */


#ifndef _CONFIG_H_
#define _CONFIG_H_

/***** GLOBAL VARIABLES *****/

extern char config_file_4_res[];
extern char default_sec_config_file[];


/***** GLOBAL FUNCTIONS *****/

extern int config_init(char *dirname);

/**** SNMP security(5-13-96) ***/
extern int sec_config_init(char *filename);


extern int personal_file_reading(char* dirname, char* filename, time_t *file_time);
extern int resource_update(char *filename);
#endif

