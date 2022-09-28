/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)udp-svr4.c	1.13 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)udp-svr4.c	1.13 96/07/23 Sun Microsystems";
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
#include  <netdb.h>
#include  <netinet/udp_var.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define CACHE_LIFETIME 45
#define INSTANCE_LEN 5
#define LOCAL_ADDR 0
#define LOCAL_PORT 4

mib2_udp_t *udp;
mib2_udpEntry_t *udp_table; 
mib_item_t *item_udp = NULL;
static time_t udp_cache_time = 0;
static int udpconns=0;

void
pr_udp()
{
	struct hostent *hp;
	int i;
	mib2_udpEntry_t *ct;

	printf("---- udp ---  table size:  %d\n", udpconns);
	printf(" udpInDatagrams %d\n",udp->udpInDatagrams);
	printf(" udpInErrors %d\n",udp->udpInErrors);
	printf(" udpOutDatagrams %d\n",udp->udpOutDatagrams);
	printf(" udpEntrySize %d\n\n",udp->udpEntrySize);
	printf("host     port     state\n");
	for (i=1,ct = udp_table;i<=udpconns && ct; ct++,i++) {
		struct in_addr in;
		in.s_addr = ct->udpLocalAddress;
		printf("%-15s     %3d    %3d\n",inet_ntoa(in),
			ct->udpLocalPort,ct->udpEntryInfo.ue_state);
	}
}

int
read_udp()
{
mib_item_t *item;
mib2_udpEntry_t *ct;

if ((cache_now - udp_cache_time) <= CACHE_LIFETIME)
   return 0;
udp = NULL;
udp_cache_time = cache_now;

if (item_udp)
   mibfree(item_udp);
if ((item_udp = (mib_item_t *)mibget(MIB2_UDP)) == NULL)
   return -1;
for (item = item_udp ; item; item=item->next_item) {
   if (item->group == MIB2_UDP)  {
      udp = (mib2_udp_t *)item->valp;
      break;
   }
}
if (!udp) {
   mibfree(item_udp);
   return -1; 
}
for (item = item_udp ; item; item=item->next_item) {
   if (item->group == MIB2_UDP && item->mib_id == MIB2_UDP_5)  {
      udp_table = (mib2_udpEntry_t *)item->valp;
      break;
   }
}
if (!udp_table)
{
  mibfree(item_udp);
  return -1;
}
ct = udp_table;
udpconns = 0;
while ((char *)ct < item->valp + item->length) {
        udpconns++;
        ct++;
}
return 0;

}

/*
 * BUG #1174715
 * QuickDump and event request for snmp->udp, snmp-mibII->udp
 * return 0 for all attributes. This routine is get_udp_uints is
 * added here and in udp.c (for SUNOS) to retreive the data.
 * This routine is also defined in sun-mib.asn file
 */
 
/*ARGSUSED*/
UINT_32_T get_udp_uints(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{

extern mib2_ip_t *ips;
extern int read_ip_cache();

if (read_udp() == -1) return -1;

switch (lastmatch)
  {
  case 1:	/* udpInDatagrams		*/
      return (INT_32_T)udp->udpInDatagrams;
  case 2:	/* udpNoPorts	*/
      read_ip_cache() ;
      return (INT_32_T)ips->udpNoPorts ;
  case 3:	/* udpInErrors	*/
      return (INT_32_T)udp->udpInErrors ;
  case 4:	/* udpOutDatagrams	*/
      return (INT_32_T)udp->udpOutDatagrams ;
  case 5:	/* udpEntrySize	*/
      return (INT_32_T)udp->udpEntrySize ;
  default:
      return (INT_32_T)0;
  }
/*NOTREACHED*/
}

/*ARGSUSED*/
UINT_32_T
  get_udpInErrors(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    return (UINT_32_T)udp->udpInErrors;
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
mib2_udpEntry_t *ct;
int i;

/* There must be exactly 5 unused components */
if (compc != INSTANCE_LEN) return -1;

/* The unused component must represent a valid IP address and UDP port # */
localip = frungulate(4, compl+LOCAL_ADDR);
localport = (unsigned int)compl[LOCAL_PORT];

if (last_match > 2)
  return -1;

if ((udp == (mib2_udp_t *)0) ||
    ((cache_now - udp_cache_time) > CACHE_LIFETIME))
  {
  udp_cache_time = cache_now;
  if (read_udp() == -1)
	return -1;
  }

for (i=0,ct = udp_table;i<udpconns && ct; ct++,i++) {
    if ((localport == ct->udpLocalPort) &&
	(localip.s_addr == ct->udpLocalAddress))
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
mib2_udpEntry_t *ct;
mib2_udpEntry_t *best;
int		bra[INSTANCE_LEN], lra[INSTANCE_LEN], pra[INSTANCE_LEN];
int		i,j;
struct in_addr in;

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

if ((udp == (mib2_udp_t *)0) ||
    ((cache_now - udp_cache_time) > CACHE_LIFETIME))
  {
  udp_cache_time = cache_now;
  if (read_udp() == -1)
        return -1;
  }

/* Now find the lowest value larger than localip and localport. */
best = (mib2_udpEntry_t  *)0;
for (i=0,ct = udp_table;i<udpconns && ct; ct++,i++) {
    in.s_addr = ct->udpLocalAddress;
    ip_to_rlist(in, (OIDC_T *)(pra+LOCAL_ADDR));
    pra[LOCAL_PORT] = ct->udpLocalPort;
    if (objidcmp(pra, lra, INSTANCE_LEN) <= 0)
      continue;
    if (!best || (objidcmp(pra, bra, INSTANCE_LEN) < 0)) {
	best = ct;
	for (j = 0; j < INSTANCE_LEN; j++)
	  bra[j] = pra[j];
    }
}

if (best) {
    in.s_addr = best->udpLocalAddress;
    ip_to_rlist(in, rlist+LOCAL_ADDR);
    rlist[LOCAL_PORT] = best->udpLocalPort;
    return INSTANCE_LEN;
}

/* Pass on it */
return 0;
}

int
udp_init()
{
if (mibopen() == 0)
  return -1;
read_udp();
return 0;
}
