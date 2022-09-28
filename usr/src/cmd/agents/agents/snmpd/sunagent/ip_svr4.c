/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)ip-svr4.c	1.15 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)ip-svr4.c	1.15 96/10/02 Sun Microsystems";
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
 * $Log:   D:/snmpv2/agent/sun/ip.c_v  $
 * 
 *    Rev 2.1   27 Nov 1990 16:58:26
 * Fixed "ipforwarding" to match Sun OS 4.1, also removed some
 * debugging printfs.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:22
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
#include  <netinet/ip_var.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define	then

#define INSTANCE_LEN 4

#define CACHE_LIFETIME 45
static time_t ip_cache_time = 0;
mib2_ip_t *ips;
mib_item_t *item_ip = NULL;
static time_t ipforw_cache_time = 0;
static int    ipforw_val = 0;

int	sun_os_ver = 41;	/* Assume Sun OS 4.1 or above */

void pr_ip()
{
   printf("---- ip ----\n");
   printf("ipForwarding: %d\n",ips->ipForwarding);
   printf("ipDefaultTTL: %d\n",ips->ipDefaultTTL);
   printf("ipInReceives: %d\n",ips->ipInReceives);
   printf("ipInHdrErrors: %d\n",ips->ipInHdrErrors);
   printf("ipInAddrErrors: %d\n",ips->ipInAddrErrors);
   printf("ipForwDatagrams: %d\n",ips->ipForwDatagrams);
   printf("ipInUnknownProtos: %d\n",ips->ipInUnknownProtos);
   printf("ipInDiscards: %d\n",ips->ipInDiscards);
   printf("ipInDelivers: %d\n",ips->ipInDelivers);
   printf("ipOutRequests: %d\n",ips->ipOutRequests);
   printf("ipOutDiscards: %d\n",ips->ipOutDiscards);
   printf("ipOutNoRoutes: %d\n",ips->ipOutNoRoutes);
   printf("ipReasmTimeout: %d\n",ips->ipReasmTimeout);
   printf("ipReasmReqds: %d\n",ips->ipReasmReqds);
   printf("ipReasmOKs: %d\n",ips->ipReasmOKs);
   printf("ipReasmFails: %d\n",ips->ipReasmFails);
   printf("ipFragOKs: %d\n",ips->ipFragOKs);
   printf("ipFragFails: %d\n",ips->ipFragFails);
   printf("ipFragCreates: %d\n",ips->ipFragCreates);
   printf("ipRoutingDiscards: %d\n",ips->ipRoutingDiscards);
   printf("tcpInErrs: %d\n",ips->tcpInErrs);
}

int
read_ip_cache()
{
mib_item_t *item;

if ((cache_now - ip_cache_time) <= CACHE_LIFETIME)
   return 0;

ip_cache_time = cache_now;
if (item_ip)
   mibfree(item_ip);
if ((item_ip = (mib_item_t *)mibget(MIB2_ICMP)) == NULL)
   return -1;
for (item = item_ip ; item; item=item->next_item) 
   {
   if (item->group == MIB2_IP)  
     {
      ips = (mib2_ip_t *)item->valp;
      break;
     }
   }
if (!item) {
  mibfree(item_ip);
  return -1;
}

return 0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T get_ipDefaultTTL(OIDC_T lastmatch, int compc,
		   OIDC_T *compl, char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T get_ipDefaultTTL(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
return ips->ipDefaultTTL;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T get_ip(OIDC_T lastmatch, int compc,
		   OIDC_T *compl, char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
UINT_32_T get_ip(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
if (read_ip_cache() == -1) return (UINT_32_T)0;

switch (lastmatch)
  {
  case 3:	/* ipInReceives		*/
	return (UINT_32_T)ips->ipInReceives;


  case 4:	/* ipInHdrErrors	*/
	return (UINT_32_T)ips->ipInHdrErrors;

  case 5:	/* ipInAddrErrors	*/
	return ips->ipInAddrErrors;	/* ***>>>TEMPORARY <<<*** */

  case 6:	/* ipForwDatagrams	*/
	return (UINT_32_T)ips->ipForwDatagrams;

  case 9:	/* ipInDelivers		*/
	return (UINT_32_T)ips->ipInDelivers;

  case 7:	/* ipInUnknownProtos	*/
	return (UINT_32_T)ips->ipInUnknownProtos;

  case 8:	/* ipInDiscards		*/
	return (UINT_32_T)ips->ipInDiscards;

  case 10:	/* ipOutRequests	*/
	return (UINT_32_T)ips->ipOutRequests;

  case 11:	/* ipOutDiscards	*/
	return (UINT_32_T)ips->ipOutDiscards;

  case 12:	/* ipOutNoRoutes		*/
	return (UINT_32_T)ips->ipOutNoRoutes;

  case 13:	/* ipReasmTimeout	*/
	return (UINT_32_T)ips->ipReasmTimeout;

  case 14:	/* ipReasmReqds		*/
	return (UINT_32_T)ips->ipReasmReqds;

  case 15:	/* ipReasmOKs		*/
	return (UINT_32_T)ips->ipReasmOKs;

  case 16:	/* ipReasmFails		*/
	return (UINT_32_T)ips->ipReasmFails;

  case 17:	/* ipFragOKs		*/
	return (UINT_32_T)ips->ipFragOKs;

  case 18:	/* ipFragFails		*/
	return (UINT_32_T)ips->ipFragFails;

  case 19:	/* ipFragCreates	*/
	return (UINT_32_T)ips->ipFragCreates;

  default:
	return 0;
  }
/*NOTREACHED*/
}

/************************************************************************/
/*                                                                      */
/* The following is a sample test procedure which performs a range      */
/* test on the value proposed for a set.                                */
/*                                                                      */
/* Test procedures may return the following status codes:               */
/*   0: Everything is OK                                                */
/*  -1: A noSuch (or readOnly) error                                    */
/*  -2: A genErr error                                                  */
/*  -3: A badValud error                                                */
/*                                                                      */
/************************************************************************/
int
test_ipForwarding(form, lastmatch, compc, compl, cookie, pktp, index)
	  int		form;		/* TEST_SET & TEST_GET from mib.h */
	  OIDC_T	lastmatch;	/* Last component matched */
	  int		compc;
	  OIDC_T *	compl;
	  char *	cookie;
	  SNMP_PKT_T *	pktp;
	  int		index;		/* Index (zero based) of VB_T	*/
{
VB_T *	vbp;

/* For non-tabular parameters, the "object instance", i.e. the unused	*/
/* portion of the object identifier, must be a single component of value*/
/* zero.								*/
if (!((compc == 1) && (*compl == 0)))
   then return -1;	/* Return a noSuch error */

if (form == TEST_GET) then return 0;

/* Make sure that the range is valid */
if ((vbp = index_to_vbp(pktp, index)) == (VB_T *)0)
   then return -1;	/* Return a noSuch error */

if ((vbp->value_u.v_number < 1) || (vbp->value_u.v_number > 2))
   then return -3;	/* Return a badValue error */

return 0;
}

/*ARGSUSED*/
INT_32_T get_ipForwarding(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
return (INT_32_T)ips->ipForwarding;
}

/*ARGSUSED*/
void
set_ipForwarding(lastmatch, compc, compl, cookie, value, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char		*cookie;
	INT_32_T	value;
	SNMP_PKT_T *	pktp;
	int		index;
{
/* JFN mibset ,use ioctl to /dev/ip */
/*
if (find_loc(ipfwp) != 0)
   {
   (void) write_int((off_t)(ipfwp->offset), (int)((value == 2) ? 0 : 1));
   }
*/
}

int
ip_init()
{
if (mibopen() == 0)
   return -1;
read_ip_cache();
return 0;
}

/*
 * IP Address Table
 */

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ipAdEntAddr(OIDC_T lastmatch,
		     int compc,
		     OIDC_T *compl,
		     char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ipAdEntAddr(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
    static struct in_addr ipaddr;

    /* The unused component must represent a valid IP address */
    ipaddr = frungulate(4, compl);

    return (unsigned char *)(&ipaddr);
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ipAdEntIfIndex(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ipAdEntIfIndex(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
    struct lif *lifp;

    if ((lifp = getlifbycompl(compc, compl)) != LIF_NULL)
       return getlifindex(lifp);

    /* This shouldn't happen because the interface was checked by the
     * test function already. */
    return 0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ipAdEntNetMask(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ipAdEntNetMask(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
    struct lif *lifp;
    static unsigned long foo = 0;
    static struct in_addr in;
    struct in_addr out;

    if ((lifp = getlifbycompl(compc, compl)) != LIF_NULL) {
	in.s_addr = lifp->ipAddr->ipAdEntNetMask;
#ifdef i386
	out.s_addr = inet_ntoa(in.s_addr);
#endif
	return (unsigned char *)&in;
    }

    /* This shouldn't happen because the interface was checked by the
     * test function already. */
    return (unsigned char *)&foo;
}

#if 0
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ipAdEntBcastAddr(OIDC_T lastmatch,
			  int compc,
			  OIDC_T *compl,
			  char *cookie)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ipAdEntBcastAddr(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif	/* NO_PP */
{
    struct lif *lifp;
    int i;

    if ((lifp = getlifbycompl(compc, compl)) != LIF_NULL)
       return lifp->ipAddr->ipAdEntBcastAddr; 

    /* This shouldn't happen because the interface was checked by the
     * test function already. */
    return 1;
}
#endif

/****************************************************************************
NAME:  ipadtable_test

PURPOSE:  Test whether a given object exists in the IP Address table

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
ipadtable_test(form, last_match, compc, compl, cookie)
int	form;		/* See TEST_SET & TEST_GET in mib.h	*/
OIDC_T	last_match;	/* Last component matched */
int	compc;
OIDC_T	*compl;
char	*cookie;
{
    struct lif *	lifp;

    /* There must be exactly 4 unused components */
    if (compc != INSTANCE_LEN)
      return -1;

    if (last_match > 5)
      return -1;

    get_if_info();

    if ((lifp = getlifbycompl(compc, compl)) != LIF_NULL)
	  return 0;

    return -1;
}

/****************************************************************************
NAME:  ipadtable_next

PURPOSE:  Locate the "next" object in the IP Address table

PARAMETERS:
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.
			The following values give an example:
				1 - ipAdEntAddr
				2 - ipAdEntIfIndex
				3 - ipAdEntNetMask
				4 - ipAdEntBcastAddr
				5 - ipAdEntReasmMaxSize

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
ipadtable_next(last_match, tcount, tlist, rlist, cookie)
OIDC_T	last_match;	/* Last component matched */
int	tcount;
OIDC_T	*tlist;
OIDC_T	*rlist;
char	*cookie;
{
    struct lif *	lifp;
    struct lif *	best;
    int		bra[INSTANCE_LEN], lra[INSTANCE_LEN], pra[INSTANCE_LEN];
    int		i;
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

    get_if_info();
    /* Now find the lowest value larger than the given instance */
    best = 0;
    for (lifp = lif; lifp; lifp = lifp->next) {
        if (lifp->flags & LIF_NO_ADDRESS) then continue;
	in.s_addr = lifp->ipAddr->ipAdEntAddr;
	ip_to_rlist(in, (OIDC_T *)pra);

	if (objidcmp(pra, lra, INSTANCE_LEN) <= 0)
	  continue;

	if (!best || (objidcmp(pra, bra, INSTANCE_LEN) < 0)) {
	    best = lifp;
	    for (i = 0; i < INSTANCE_LEN; i++)
	      bra[i] = pra[i];
	}
    }

    if (best) {
	for (i = 0; i < INSTANCE_LEN; i++)
	  rlist[i] = bra[i];
	return INSTANCE_LEN;
    }

    /* Pass on it */
    return 0;
}
