/* Copyright 1988 - 09/20/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)sendtrap.c	2.16 96/09/20 Sun Microsystems"
#else
static char sccsid[] = "@(#)sendtrap.c	2.16 96/09/20 Sun Microsystems";
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
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header	*/
/*
 * $Log:   C:/snmpv2/agent/sun/sendtrap.c_v  $
 * 
 *    Rev 2.1   28 May 1990 16:07:46
 * Corrected error in which address of Get_sysUptime was being passed
 * rather then the result of that routine.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:14
 * Initial revision.
*/

#include <stdio.h>
#include <sys/types.h>
/* #include <time.h> */
#include <sys/time.h>

#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>
#include "snmpvars.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include "agent.h"

#include <print.h>
#define then

extern int errno;

/* Set up an OID for use with the linkUp and linkDown traps */
static OIDC_T	ifIndex_oid[] = {
			   	/* ISO */		1,
			   	/* ORG */		3,
			   	/* DOD */		6,
			   	/* INTERNET */		1,
			   	/* MGMT */		2,
			   	/* MIB */		1,
			   	/* INTERFACES */	2,
			   	/* ifTable */		2,
			   	/* ifEntry */		1,
			   	/* ifIndex */		1
				};
static int	ifIndex_oid_count = sizeof(ifIndex_oid)/sizeof(OIDC_T);
/* Say how far into ifIndex_oid is the value component ifIndex */
#define	ifIndex_index	9

#if !defined(NO_PP)
static	void	send_a_trap(int, int, u_long, int);
#else	/* NO_PP */
static	void	send_a_trap();
#endif	/* NO_PP */

void
send_traps(fromsock, ttype, ifnum)
	int	fromsock;
	int	ttype;
	int	ifnum;		/* For linkup/linkdown, the interface # */
{
int	thnum;

for (thnum = 0; thnum < trap_2_cnt; thnum++)
   {
   send_a_trap(fromsock, ttype, traplist[thnum], ifnum);
   }
}

static
void
send_a_trap(fromsock, ttype, host_addr, ifnum)
	int	fromsock;
	int	ttype;
	u_long	host_addr;
	int	ifnum;
{
EBUFFER_T		ebuff;
struct sockaddr_in	traps_to;
extern	UINT_32_T	get_sysUpTime();

EBufferInitialize(&ebuff);

/* Generate and send a generic trap. */
  {
  SNMP_PKT_T	*trap_pkt;
  struct timeval now_is;

   (void) gettimeofday(&now_is, (struct timezone *)0);

  if ((trap_pkt = SNMP_Create_Trap(VERSION_RFC1098,
			strlen(snmp_trap_community), snmp_trap_community,
			snmp_product_id_count, snmp_product_id,
			(OCTET_T *)snmp_local_ip_address,
			ttype, (INT_32_T)0,
			(UINT_32_T)get_sysUpTime(),
			ifnum == 0 ? 0 : 1))
      			!= (SNMP_PKT_T *)0)
     then {
          if (ifnum != 0)
	     then {
	          ifIndex_oid[ifIndex_index] = ifnum;
	          (void) SNMP_Bind_Integer(trap_pkt, 0, ifIndex_oid_count,
					     ifIndex_oid, (INT_32_T)ifnum);
	          }
	  if (SNMP_Encode_Packet(trap_pkt, &ebuff) == -1)
	     then {
		  PRNTF0("Failure encoding startup trap\n");
		  }
	  if (trace_level > 1)
	     then {
	          struct in_addr toaddr;
		  toaddr.s_addr = host_addr;
	          printf("Trap packet sent to %s at %s",
			 inet_ntoa(toaddr),
			 ctime((time_t *)&now_is.tv_sec));
		  print_pkt(trap_pkt);
		  fflush(stdout);

		  }
	  SNMP_Free(trap_pkt);
	  }

  /* Transmit the Trap PDU to the trap port on the trap catching machine. */
  /* Note that in real life traps may be sent to more than one catcher.   */

  traps_to.sin_family = AF_INET;
  traps_to.sin_port = htons(162);
  traps_to.sin_addr.s_addr = host_addr;

  (void) sendto(fromsock, (char *)ebuff.start_bp, EBufferUsed(&ebuff),
	        0, (struct sockaddr *)&traps_to, sizeof(traps_to));
#if defined(SGRP)
  inc_counter(snmp_stats.snmpOutPkts);
  inc_counter(snmp_stats.snmpOutTraps);
#endif
  }

EBufferClean(&ebuff);
}
