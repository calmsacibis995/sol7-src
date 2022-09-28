/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)udp.c	2.16 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)udp.c	2.16 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/UDP.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:26
 * Initial revision.
 * 
*/

#include <stdio.h>
#include <time.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/types.h>
#include  <sys/socket.h>
#include  <net/route.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#include  <netinet/in_systm.h>
#include  <netinet/ip.h>
#include  <netinet/ip_var.h>
#include  <netinet/in_pcb.h>
#include  <netinet/udp.h>
#include  <netinet/udp_var.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define CACHE_LIFETIME 45
#define INSTANCE_LEN 5
#define LOCAL_ADDR 0
#define LOCAL_PORT 4

static struct kernel_symbol udpstat = { "_udpstat", 0, 0};
static struct udpstat udps;

static struct kernel_symbol udp_cons = { "_udb", 0, 0 };
static struct inpcb *udb = 0;
static time_t udp_cache_time = 0;

/*ARGSUSED*/
UINT_32_T
  get_udpInErrors(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    if (find_loc(&udpstat) == 0)
      return (UINT_32_T)0;

    read_bytes((off_t)udpstat.offset, (char *)&udps,
	       sizeof(struct udpstat));
    return (UINT_32_T)(udps.udps_hdrops + udps.udps_badsum +
		      udps.udps_badlen);
}

/*ARGSUSED*/
INT_32_T
  get_udptab_localport(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    return (INT_32_T)(compl[LOCAL_PORT]);
}

/*ARGSUSED*/
char *
  get_udptab_ipaddr(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
static struct in_addr	localip;

/* The unused component must represent a valid IP address and UDP port # */
localip = frungulate(4, compl+LOCAL_ADDR);

    return (char *)(&localip);
}

/*
 * BUG #1174715
 * QuickDump and event request for snmp->udp, snmp-mibII->udp
 * return 0 for all attributes. This routine is get_udp_uints is
 * added in udp-svr4.c and in udp.c (for SUNOS) to retreive the data.
 * This routine is also defined in sun-mib.asn file
 * Since the SUNOS kernel doesn't provide support to get these
 * attributes we return 0 for all attributes except errors
 */
 
/*ARGSUSED*/
UINT_32_T get_udp_uints(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{

switch (lastmatch)
  {
  case 3:	/* udpInErrors	*/
	return (UINT_32_T)get_udpInErrors(lastmatch, compc, compl, cookie) ;
  case 1:	/* udpInDatagrams		*/
  case 2:	/* udpNoPorts	*/
  case 4:	/* udpOutDatagrams	*/
  case 5:	/* udpEntrySize	*/
  default:
      return (INT_32_T)0;
  }
/*NOTREACHED*/
}

/****************************************************************************
NAME:  udptable_test

PURPOSE:  Test whether a given object exists in the UDP table

PARAMETERS:
	int		TEST_SET or TEST_GET indicating what type of
			access is intended.
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This is usually
			the identifier for the particular attribute
			in the table.
	int		Number of components in the unused part of the
			object identifier
	OIDC_T *	Unused part of the object identifier
	char *		User's cookie, passed unchanged from the LEAF macro
			in the mib.c

RETURNS:  int		0 if the attribute is accessable.
			-1 if the attribute is not accessable.
****************************************************************************/
/*ARGSUSED*/
int
udptable_test(form, last_match, compc, compl, cookie)
	int	form;	/* See TEST_SET & TEST_GET in mib.h	*/
	OIDC_T	last_match; /* Last component matched */
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
{
struct in_addr	localip;
unsigned int	localport;
struct inpcb *	pcb;

/* There must be exactly 5 unused components */
if (compc != INSTANCE_LEN) return -1;

/* The unused component must represent a valid IP address and UDP port # */
localip = frungulate(4, compl+LOCAL_ADDR);
localport = (unsigned int)compl[LOCAL_PORT];

if (last_match > 2)
  return -1;

if ((udb == (struct inpcb *)0) ||
    ((cache_now - udp_cache_time) > CACHE_LIFETIME))
  {
  udp_cache_time = cache_now;
  if (find_loc(&udp_cons) == 0)
    return -1;
  (void) read_ct(&udb, udp_cons.offset);
  }

for (pcb = udb; pcb; pcb = pcb->inp_next) {
    if ((localport == pcb->inp_lport) &&
	(localip.s_addr == pcb->inp_laddr.s_addr))
      return 0;
}
return -1;
}

/****************************************************************************
NAME:  udptable_next

PURPOSE:  Locate the "next" object in the UDP table

PARAMETERS:
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.
			The following values give an example:
				 1 - udpLocalAddres
				 2 - udpLocalPort

	int		Number of components in the yet unused part of the
			object identifier
	OIDC_T *	Yet unused part of the object identifier
	OIDC_T *	Start of an area in which the object instance part
			of the name of the "next" object should be
			constructed.
	char *		User's cookie, passed unchanged from the LEAF macro
			in the mib.c

RETURNS:  int		>0 indicates how many object identifiers have
			been placed in the result list.
			0 if there is no "next" in the table

This routine's job is to use the target object instance, represented
by variables tcount and tlist, to ascertain whether there is a "next"
entry in the table.  If there is, the object instance of that element
is constructed in the return list area (rlist) and the number of elements
returned.  If there is no "next" a zero is returned.

It is possible that the target object instance may be empty (i.e.
tcount == 0) or incomplete (i.e. tcount < sizeof(normal object instance)).

If tcount == 0, the first object in the table should be returned.

The term "object instance" refers to that portion of an object identifier
used by SNMP to identify a particular instance of a tabular variable.
****************************************************************************/
/*ARGSUSED*/
int
udptable_next(last_match, tcount, tlist, rlist, cookie)
	OIDC_T	last_match; /* Last component matched */
	int	tcount;
	OIDC_T	*tlist;
	OIDC_T	*rlist;
	char	*cookie;
{
struct inpcb *	pcb;
struct inpcb *	best;
int		bra[INSTANCE_LEN], lra[INSTANCE_LEN], pra[INSTANCE_LEN];
int		i;

/* Read tlist into an array with each segment larger than the value can be
 * so -1 can represent a value smaller than any legal value. */
for (i = 0; i < INSTANCE_LEN; i++) {
    if (tcount) {
	lra[i] = tlist[i];
	tcount--;
    }
    else
      lra[i] = -1;
}

if ((udb == (struct inpcb *)0) ||
    ((cache_now - udp_cache_time) > CACHE_LIFETIME))
  {
  udp_cache_time = cache_now;
  if (find_loc(&udp_cons) == 0)
    return 0;
  (void) read_ct(&udb, udp_cons.offset);
  }

/* Now find the lowest value larger than localip and localport. */
best = (struct inpcb *)0;
for (pcb = udb; pcb; pcb = pcb->inp_next) {
    ip_to_rlist(pcb->inp_laddr, (OIDC_T *)(pra+LOCAL_ADDR));
    pra[LOCAL_PORT] = pcb->inp_lport;
    if (objidcmp(pra, lra, INSTANCE_LEN) <= 0)
      continue;
    if (!best || (objidcmp(pra, bra, INSTANCE_LEN) < 0)) {
	best = pcb;
	for (i = 0; i < INSTANCE_LEN; i++)
	  bra[i] = pra[i];
    }
}

if (best) {
    ip_to_rlist(best->inp_laddr, rlist+LOCAL_ADDR);
    rlist[LOCAL_PORT] = best->inp_lport;
    return INSTANCE_LEN;
}

/* Pass on it */
return 0;
}

int
udp_init()
{
if (find_loc(&udpstat) == 0) return -1;
if (find_loc(&udp_cons) == 0) return -1;
return 0;
}
