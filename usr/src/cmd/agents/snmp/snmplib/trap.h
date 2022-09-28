/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trap.h	1.4 96/07/01 Sun Microsystems"

#ifndef _TRAP_H_
#define _TRAP_H_


#include "pdu.h"


/***** GLOBAL VARIABLES *****/

extern char *trap_community;

extern Subid sun_subids[];
extern Oid sun_oid;


/***** GLOBAL FUNCTIONS *****/

extern int trap_init(Oid *default_enterprise, char *error_label);

extern int trap_send(IPAddress *ip_address, Oid *enterprise, int generic, int specific, SNMP_variable *variables, char *error_label);
extern int trap_send_with_more_para(IPAddress *ip_address,
									IPAddress my_ip_addr,
									int i_flag,
									Oid *enterprise,
									int generic,
									int specific,
									int trap_port,
									u_long time_stamp,
									SNMP_variable *variables,
									char *error_label);
extern int trap_destinator_add(char *name, char *error_label);
extern void delete_trap_destinator_list();
extern void trace_trap_destinators();
extern int trap_send_to_all_destinators(Oid *enterprise, int generic, int specific, SNMP_variable *variables, char *error_label);
extern int trap_send_to_all_destinators7(int i_flag, Oid *enterprise, int generic, int specific, u_long time_stamp, SNMP_variable *variables, char *error_label);
extern int trap_send_raw(IPAddress *ip_address, IPAddress my_ip_addr,
        char* community,int i_flag,Oid *enterprise,int generic,
        int specific,int trap_port,u_long time_stamp,
        SNMP_variable *variables,char *error_label);


#endif
