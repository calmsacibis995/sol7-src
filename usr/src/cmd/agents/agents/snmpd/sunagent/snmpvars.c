/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)snmpvars.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)snmpvars.c	2.15 96/07/23 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1988  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/AGENT/SUN/SNMPVARS.C_V   2.0   31 Mar 1990 15:34:16  $	*/
/*
 * $Log:   E:/SNMPV2/AGENT/SUN/SNMPVARS.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:16
 * Initial revision.
 * 
 *    Rev 1.0   10 Oct 1988 21:48:48
 * Initial revision.
*/

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if defined(SGRP)
#if (!defined(snmp_inc))
#include <snmp.h>
#endif
#endif	/* SGRP */

#include <sys/types.h>
#include "snmpvars.h"

#include <stdio.h>
#include <string.h>
#include <memory.h>

#include <sys/socket.h>
#include <ctype.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netdb.h>
/* #include <arpa/inet.h> */
#include <fcntl.h>
/* #include <time.h> */
#include <sys/time.h>

#include "general.h"
#include "agent.h"

/**********************************************************************
 *
 * Define the various SNMP management variables used in this system.
 *
 * This file should closely match snmpvars.h
 *
 **********************************************************************/

char	snmp_sysgrp_read_community[SNMP_COMM_MAX] = "public";
char	snmp_sysgrp_write_community[SNMP_COMM_MAX] = "private";
char	snmp_fullmib_read_community[SNMP_COMM_MAX] = "all public";
char	snmp_fullmib_write_community[SNMP_COMM_MAX] = "all private";

char	snmp_trap_community[SNMP_COMM_MAX] = "SNMP_trap";

char	snmp_auth_traps = 1;

OIDC_T	snmp_product_id[] = {
			   /* ISO */			1,
			   /* ORG */			3,
			   /* DOD */			6,
			   /* INTERNET */		1,
			   /* PRIVATE */		4,
			   /* ENTERPRISES */		1,
			   /* SUN */			42,
			   /* SUN PRODUCTS */		2,
			   /* SNMP AGENT */		1,
			   /* VERSION 1 */		1,
			   };

int		snmp_product_id_count =
				sizeof(snmp_product_id)/sizeof(OIDC_T);

unsigned char	snmp_local_ip_address[4] = {128, 224, 0, 0};

/* WARNING: The following strings should be in NVT form, i.e. any embedded */
/* newlines whould be carriage-return followed by linefeed (newline)       */
char	snmp_sysDescr[MAX_SYSDESCR] =
#if defined(SVR4)
			"Epilogue Technology SNMP agent for Sun SVR4";
#else	/* (SVR4) */
			"Epilogue Technology SNMP agent for Sun OS 4.1 and 4.1.1";
#endif	/* (SVR4) */

char	snmp_sysContact[MAX_SYSCONTACT] = "sysContact not set";

char	snmp_sysLocation[MAX_SYSLOCATION] = "sysLocation not set";

char	snmp_sysName[MAX_SYSNAME] = "sysName not set";

char	kernel_file[MAX_KERN_FILE] =
#if defined(SVR4)
					"/unix";
#else	/* (SVR4) */
					"/vmunix";
#endif	/* (SVR4) */

OBJ_ID_T	snmp_sysObjectID = {sizeof(snmp_product_id)/sizeof(OIDC_T),
				    snmp_product_id };

struct timeval	boot_at = {0L, 0L};

char	config_file[MAX_CONFIG_FILE] = CONFIG_FILE;

int	trap_2_cnt = 0;		/* How many entries follow */
u_long	traplist[MAX_TRAPS_TO];	/* IP addresses where to send traps */

int	mgr_cnt = 0;		/* How many entries follow */
u_long	mgr_list[MAX_MGR];	/* IP addresses for valid managers */

#if 0
int	refresh_minutes = 0;	/* How many minutes agent can live w/o	*/
				/*  an incoming packet.			*/
				/*  Zero means forever.			*/
#endif /* 0 */

time_t	cache_now;		/* Time of each query.			*/

int	trace_level = 0;	/* Execution trace level */
int	read_only = 0;		/* Set != 0 if writes are to be blocked */
int	snmp_socket;		/* Socket used to send traps and	*/
				/* send/receive SNMP queries, usually	*/
				/* UDP port 161				*/

#if defined(SGRP)
SNMP_STATS_T	snmp_stats;
#endif	/* SGRP */

int
snmpvars_init()
{
#if defined(SGRP)
(void)memset((char *)&snmp_stats, 0, sizeof(snmp_stats));
#endif	/* SGRP */

/* Leave setting of the trap socket desciptor for agent_body()	*/
return 0;
}
