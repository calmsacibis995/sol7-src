/* Copyright 07/16/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)snmpd.h	1.4 96/07/16 Sun Microsystems"

/* HISTORY
 * 5-20-96	Jerry Yeung	support security file and subtree reg.
 */

#ifndef _SNMPD_H_
#define _SNMPD_H_

extern char *config_file;
extern char *sec_config_file;
extern int agent_port_number;
extern int max_agent_reg_retry;

extern void SSAMain(int argc, char** argv);
#endif
