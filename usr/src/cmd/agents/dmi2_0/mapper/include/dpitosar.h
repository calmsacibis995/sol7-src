/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dpitosar.h	1.1 96/08/05 Sun Microsystems"


#define NUM_AGENTRELAY_RETRIES     10
#define SUBAGENT_PORT              6500

extern int subagent_register(Agent *subagent);
extern int subagent_unregister(Agent *subagent);
extern int open_socket(struct sockaddr_in *in);
extern int DPIconnect_to_agent_UDP(char *hostname_p , char *community_p ); 
extern int agent_alive();
extern int subtree_register(int agentid, char *oidstr, int treeindex, 
       int timeout, long priority, char bulk_select, char view_select ); 
extern int subtree_unregister(int agentid, char *oidstr, int treeindex); 
extern int open_socket(struct sockaddr_in *in); 
extern snmp_dpi_hdr * pdu_to_dpihdr(SNMP_pdu *pdu); 
extern SNMP_pdu * dpihdr_to_pdu(snmp_dpi_hdr *dpihdr, snmp_dpi_set_packet *setp, u_long error_status, u_long error_index); 
extern void fDPIget_packet(snmp_dpi_get_packet *pack_p);
extern void fDPInext_packet(snmp_dpi_next_packet *pack_p);
extern void fDPIset_packet(snmp_dpi_set_packet *pack_p);
extern void fDPItrap_packet(snmp_dpi_trap_packet *pack_p);
extern void fDPIresp_packet(snmp_dpi_resp_packet *pack_p);
extern void fDPIopen_packet(snmp_dpi_open_packet *pack_p);
extern void fDPIclose_packet(snmp_dpi_close_packet *pack_p);
extern void fDPIreg_packet(snmp_dpi_reg_packet *pack_p);
extern void fDPIureg_packet(snmp_dpi_ureg_packet *pack_p);
extern int  group_or_instance_invalid(char *group_p, char *instance_p, char *function_p );
extern char *copy(char *cp,int *len);
extern char *concat(char *p1, char *p2);
extern int  oid_is_invalid(char *oid_p, int strong);


