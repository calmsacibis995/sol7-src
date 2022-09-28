/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)ip.c	2.16 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)ip.c	2.16 96/07/23 Sun Microsystems";
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
static struct kernel_symbol ipstat_sym = { "_ipstat", 0, 0};
static struct ipstat ips;
static time_t ipforw_cache_time = 0;
static struct kernel_symbol ipforw_old = { "_ipforwarding", 0, 0 };
static struct kernel_symbol ipforw = { "_ip_forwarding", 0, 0 };
static struct kernel_symbol *ipfwp = &ipforw;
static int    ipforw_val = 0;

int	sun_os_ver = 41;	/* Assume Sun OS 4.1 or above */

static
int
read_ip_cache()
{
if ((cache_now - ip_cache_time) <= CACHE_LIFETIME)
   return 0;

ip_cache_time = cache_now;

if (find_loc(&ipstat_sym) == 0)
  return -1;

read_bytes((off_t)ipstat_sym.offset, (char *)&ips,
	   sizeof(struct ipstat));

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
return MAXTTL;
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
	return (UINT_32_T)ips.ips_total;


  case 4:	/* ipInHdrErrors	*/
	return (UINT_32_T)(ips.ips_badsum + ips.ips_tooshort +
			  ips.ips_toosmall + ips.ips_badhlen + ips.ips_badlen);

  case 5:	/* ipInAddrErrors	*/
	return 0;	/* ***>>>TEMPORARY <<<*** */

  case 6:	/* ipForwDatagrams	*/
	return (UINT_32_T)ips.ips_forward;

  case 9:	/* ipInDelivers		*/
	return (UINT_32_T)(ips.ips_total -
			   (ips.ips_badsum + ips.ips_tooshort +
			    ips.ips_toosmall + ips.ips_badhlen +
			    ips.ips_badlen + ips.ips_fragments +
			    ips.ips_forward + ips.ips_cantforward +
			    ips.ips_redirectsent));

  case 7:	/* ipInUnknownProtos	*/
  case 8:	/* ipInDiscards		*/
  case 10:	/* ipOutRequests	*/
  case 11:	/* ipOutDiscards	*/
	return 0;	/* ***>>>TEMPORARY <<<*** */

  case 12:	/* ipNoRoutes		*/
	return (UINT_32_T)ips.ips_cantforward;

  case 13:	/* ipReasmTimeout	*/
	return (UINT_32_T)ips.ips_fragtimeout;

  case 14:	/* ipReasmReqds		*/
	return (UINT_32_T)ips.ips_fragments;

  case 15:	/* ipReasmOKs		*/
	return 0;	/* ***>>>TEMPORARY <<<*** */

  case 16:	/* ipReasmFails		*/
	return (UINT_32_T)(ips.ips_fragtimeout + ips.ips_fragdropped);

  case 17:	/* ipFragOKs		*/
  case 18:	/* ipFragFails		*/
  case 19:	/* ipFragCreates	*/
	return 0;	/* ***>>>TEMPORARY <<<*** */

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
if ((cache_now - ipforw_cache_time) > CACHE_LIFETIME)
   {
   ipforw_cache_time = cache_now;
   if (find_loc(ipfwp) != 0)
      {
      ipforw_val = read_int((off_t)(ipfwp->offset));
      }
   }
return (INT_32_T)(ipforw_val == 0 ? 2 : 1);
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
if (find_loc(ipfwp) != 0)
   {
   (void) write_int((off_t)(ipfwp->offset), (int)((value == 2) ? 0 : 1));
    ipforw_cache_time = 0;
   }
}

int
ip_init()
{
if (find_loc(&ipstat_sym) == 0) return -1;
if (find_loc(&ipforw) != 0)
   then ipfwp = &ipforw;
   else {
        if (find_loc(&ipforw_old) != 0)
	   then {
	        sun_os_ver = 40;
		ipfwp = &ipforw_old;
		}
           else return -1;
	}
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

    if ((lifp = getlifbycompl(compc, compl)) != LIF_NULL)
	  return (unsigned char *)&(lifp->netmask);

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
       return ((struct sockaddr_in *)
	       (&(lifp->ifp->if_broadaddr)))->sin_addr.S_un.S_un_b.s_b4 & 1;

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
	ip_to_rlist(((struct sockaddr_in *)
		     (&(lifp->ifp->if_addrlist->ifa_addr)))->sin_addr,
		    (OIDC_T *)(pra));

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
