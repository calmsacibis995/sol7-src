/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)icmp.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)icmp.c	2.15 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/ICMP.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:20
 * Initial revision.
 * 
*/

#include <stdio.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/types.h>
#include  <sys/socket.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#include  <netinet/in_systm.h>
#include  <netinet/ip.h>
#include  <netinet/ip_icmp.h>
#include  <netinet/icmp_var.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

static struct icmpstat icmps;

#define CACHE_LIFETIME 45
static struct kernel_symbol icmpstat = { "_icmpstat", 0, 0};
static time_t icmp_cache_time = 0;

static
int
read_icmp()
{
if ((cache_now - icmp_cache_time) <= CACHE_LIFETIME)
   return 0;

icmp_cache_time = cache_now;

if (find_loc(&icmpstat) == 0)
   return -1;

read_bytes((off_t)icmpstat.offset, (char *)&icmps,
	   sizeof(struct icmpstat));

return 0;
}

/*ARGSUSED*/
UINT_32_T icmpin(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {

	if (read_icmp() == -1) return(UINT_32_T)0;
	return (UINT_32_T)icmps.icps_inhist[(int)cookie];
	}

/*ARGSUSED*/
UINT_32_T get_icmpInMsgs(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {
	int i;
	int total = 0;

	if (read_icmp() == -1) return(UINT_32_T)0;

	for(i = 0; i < ICMP_MAXTYPE + 1; i++)
		total += icmps.icps_inhist[i];
	total += icmps.icps_tooshort + icmps.icps_checksum;

	return (UINT_32_T)total;
	}

/*ARGSUSED*/
UINT_32_T get_icmpInErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {

	if (read_icmp() == -1) return(UINT_32_T)0;
	return (UINT_32_T)(icmps.icps_badcode + icmps.icps_tooshort +
			  icmps.icps_checksum + icmps.icps_badlen);
	}


/*ARGSUSED*/
UINT_32_T icmpout(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {

	if (read_icmp() == -1) return(UINT_32_T)0;
	return (UINT_32_T)icmps.icps_outhist[(int)cookie];
	}

/*ARGSUSED*/
UINT_32_T get_icmpOutMsgs(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {
	int i;
	int total = 0;

	if (read_icmp() == -1) return(UINT_32_T)0;

	for(i = 0; i < ICMP_MAXTYPE + 1; i++)
		total += icmps.icps_outhist[i];

	return (UINT_32_T)total;
	}

/*ARGSUSED*/
UINT_32_T get_icmpOutErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {

	if (read_icmp() == -1) return(UINT_32_T)0;
	return (UINT_32_T)(icmps.icps_oldshort + icmps.icps_oldicmp);
	}

int
icmp_init()
{
if (find_loc(&icmpstat) == 0) return -1;
return 0;
}
