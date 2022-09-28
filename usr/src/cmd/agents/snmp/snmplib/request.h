/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)request.h	1.3 96/07/01 Sun Microsystems"

/* History
 * 5-17-96 	Jerry Yeung	extern the request_create and send_blocking func.
 */

#ifndef _REQUEST_H_
#define _REQUEST_H_


/***** GLOBAL VARIABLES *****/

extern Oid sysUptime_name;
extern Oid sysUptime_instance;


/***** GLOBAL FUNCTIONS *****/

extern long request_sysUpTime(char *error_label);
extern int request_snmpEnableAuthTraps(char *error_label);
extern SNMP_pdu *request_create(char *community, int type, char *error_label);
extern SNMP_pdu *request_send_blocking(IPAddress *ip_address, SNMP_pdu *request, char *error_label);
extern SNMP_pdu *request_send_to_port_blocking(IPAddress *ip_address, int port,SNMP_pdu *request, char *error_label);
extern SNMP_pdu *request_send_to_port_time_out_blocking(IPAddress *ip_address, int port,struct timeval *timeout,SNMP_pdu *request, char *error_label);

#endif
