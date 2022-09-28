/* Copyright 09/26/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dispatcher.h	1.5 96/09/26 Sun Microsystems"

/******
 * HISTORY
 * 5-14-96	Jerry Yeung	add relay agent name
 * 6-18-96	Jerry Yeung	add pid file
 * 7-3-96	Jerry Yeung	add poll_interval & max_time_out
 */

#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_


/***** GLOBAL CONTANTS *****/

/*
 *	the two modes of the SNMP Relay
 */

#define MODE_GROUP	1
#define MODE_SPLIT	2


/***** GLOBAL VARIABLES *****/

/*
 *	my IP address
 */

extern IPAddress my_ip_address;

extern char* relay_agent_name; /* (5-14-96) */

/*
 *	the socket descriptor on which we receive/send
 *	SNMP requests from/to the SNMP applications
 */

extern int clients_sd;


/*
 *	the socket descriptor on which we receive/send
 *	SNMP requests from/to the SNMP agents
 */

extern int agents_sd;

extern int trap_sd;
extern int relay_agent_trap_port;

/*
 * max_agent_time_out
 * poll_internval
 */
extern int relay_agent_max_agent_time_out;
extern int relay_agent_poll_interval;


/*
 *
 *	the name of the configuration directory
 */

extern char *config_dir;
extern char *pid_file;
extern char *resource_file;

/*
 *	the mode of the SNMP relay
 */

extern int mode;
extern int recovery_on;


#endif
