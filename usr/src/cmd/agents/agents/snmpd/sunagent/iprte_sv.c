/* Copyright 1988 - 06/12/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)iprte-svr4.c	1.13 97/06/12 Sun Microsystems"
#else
static char sccsid[] = "@(#)iprte-svr4.c	1.13 97/06/12 Sun Microsystems";
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
 * $Log:   D:/snmpv2/agent/sun/iprte.c_v  $
 * 
 *    Rev 2.1   27 Nov 1990 16:58:52
 * Removed some debugging printfs.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:22
 * Initial revision.
 * 
*/

/*#define DEBUG*/
#include <stdio.h>
#include <time.h>
#include <memory.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/types.h>
#include  <sys/socket.h>
#include  <sys/sockio.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#define KERNEL
#include  <net/route.h>
#undef KERNEL
#include  <sys/ioctl.h>
#include  <netinet/in_systm.h>
#include  <netinet/ip.h>
#include  <netdb.h>
#include  <ctype.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define	then

#define NEXT_HOP_LOCATOR	107

#define CACHE_LIFETIME 45
#define INSTANCE_LEN 4

struct	rte
	{
	struct rte	*next;
	struct rtentry  rt_ent;
	mib2_ipRouteEntry_t *ipr;
	};

static struct   rte route_table;
mib_item_t *item_rt = NULL;
int	rt_count = 0;	/* Number of entries in route_table */

static time_t route_cache_time = 0;
#define invalidate_iprte() route_cache_time = (time_t)0

void read_routing();

extern int	errno;

void
pr_iprte()
{
struct rte *rtep;

   struct in_addr in;
   printf("---- iprte ---- %d entries\n",rt_count);
   printf("%15s  %10s  %13s\n", "Destination", "Device", "Next Hop");
   for (rtep = &route_table; rtep; rtep = rtep->next)
      {
	 in.s_addr = rtep->ipr->ipRouteDest;
	 printf("%-15s",inet_ntoa(in));
	 printf("%10s",rtep->ipr->ipRouteIfIndex.o_bytes);
	 in.s_addr = rtep->ipr->ipRouteNextHop;
         printf("\t%-15s  ",inet_ntoa(in));
	 printf("\n");
      }
}

int
iprte_init()
{
if (mibopen() == 0)
   return -1;
read_routing();
return 0;
}

void
read_routing()
{
struct rte	*rtp;
struct rte *tab,*next;
struct rte *tmp;
mib_item_t *item;
mib2_ipRouteEntry_t *np;
struct in_addr in;

if ((cache_now - route_cache_time) <= CACHE_LIFETIME)
   then return;

route_cache_time = cache_now;
if (item_rt)
   mibfree(item_rt);
if ((item_rt = (mib_item_t *)mibget(MIB2_IP)) == NULL)
   return;
for (item = item_rt ; item; item=item->next_item)
   {
   if ((item->group == MIB2_IP) && (item->mib_id == MIB2_IP_21))
      {  
        np = (mib2_ipRouteEntry_t *)item->valp;
        break;
      }  
   }   
if (!item) 
{
  mibfree(item_rt);
  return;
}
/* free up old list */
for (tab = route_table.next; tab;)
   {
   	next = tab->next;
	free(tab);
	tab = next;
   }
rtp = &route_table;
rtp->ipr = np;
np++;rt_count++;
for (; (char *)np < item->valp + item->length; np++)
   {

	if (!(tmp = (struct rte *)malloc(sizeof(struct rte))))
                return;
        tmp->next = NULL;
        rtp->next = tmp;
        rtp = tmp;
	rtp->ipr = np;
	rt_count++;
   }
}


#if !defined(NO_PP)
/*ARGSUSED*/
static
int
zap_route(struct in_addr ipaddr,   struct rte *rtep)
#else   /* NO_PP */
static
void
/*ARGSUSED*/
zap_route(ipaddr, rtep)
	struct in_addr ipaddr;
	struct rte *rtep;
#endif  /* NO_PP */
{
struct rtentry drte;

(void) memset(&drte, 0, sizeof(struct rtentry));

(void)memcpy(&drte.rt_dst, &(rtep->rt_ent.rt_dst), sizeof(struct sockaddr_in));
(void)memcpy(&drte.rt_gateway, &(rtep->rt_ent.rt_gateway),
	     sizeof(struct sockaddr_in));
drte.rt_flags = (rtep->rt_ent.rt_flags) & (RTF_UP | RTF_GATEWAY | RTF_HOST);

invalidate_iprte();
(void)ioctl(snmp_socket, SIOCDELRT, &drte);
}

#if !defined(NO_PP)
static unsigned long
  ip_net_mask(struct in_addr ia)
#else   /* NO_PP */
static unsigned long
  ip_net_mask(ia)
	struct in_addr ia;
#endif  /* NO_PP */
{
/* CAVEAT: THIS ROUTINE IS DEPENDENT ON THE HARDWARE BYTE ORDER */
    static unsigned long masks[] = {
	0xff000000,		/* Class A */
	0xff000000,		/* Class A */
	0xff000000,		/* Class A */
	0xff000000,		/* Class A */
	0xffff0000,		/* Class B */
	0xffff0000,		/* Class B */
	0xffffff00,		/* Class C */
	0xffffffff,		/* Class D */
    };

    return masks[((ia.S_un.S_un_b.s_b1 >> 5) & 0x07)];
}

#if !defined(NO_PP)
static unsigned long
route_mask(struct in_addr ipaddr)
#else   /* NO_PP */
static unsigned long
route_mask(ipaddr)
	struct in_addr ipaddr;
#endif  /* NO_PP */
{
struct lif *lp;
unsigned long netmask;
#if defined(DEBUG)
struct in_addr foo;
#endif

netmask = ip_net_mask(ipaddr);

#if defined(DEBUG)
   {
   foo.s_addr = netmask;
   printf("Start with %15s ", inet_ntoa(ipaddr));
   printf("%15s (%8X)\n", inet_ntoa(foo), netmask);
   fflush(stdout);
   }
#endif

get_if_info();
for (lp = lif; lp; lp = lp->next)
   {
#if defined(DEBUG)
      {
      foo.s_addr = ipaddr.s_addr & netmask;
      printf("   %15s  ==?  ", inet_ntoa(foo));
      foo.s_addr = lp->ipAddr->ipAdEntAddr & netmask;
      printf("  %15s\n", inet_ntoa(foo));
      fflush(stdout);
      }
#endif

   if ((ipaddr.s_addr & netmask) == (lp->ipAddr->ipAdEntAddr & netmask))
      {
#if defined(DEBUG)
	 {
/* JFN WOWOW BABAY  check out this mask shit*/
	 printf("***\n");
	 printf("Returning located mask %08.8X\n",lp->ipAddr->ipAdEntAddr);
	 fflush(stdout);
	 }
#endif
      return (unsigned long)lp->ipAddr->ipAdEntAddr;
      }
    }
#if defined(DEBUG)
PRNTF1("Returning basic mask %08.8X\n", netmask);
#endif
return netmask;
}

#if !defined(NO_PP)
static struct rte *
  ip_rte_lookup(struct in_addr ia)
#else   /* NO_PP */
static struct rte *
  ip_rte_lookup(ia)
	struct in_addr ia;
#endif  /* NO_PP */
{
struct rte *rtep;

for (rtep = &route_table; rtep != (struct rte *)0; rtep = rtep->next)
   {
   if (ia.s_addr == rtep->ipr->ipRouteDest)
      then return rtep;
   }
return (struct rte *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ipRouteDest(OIDC_T lastmatch,
		     int compc,
		     OIDC_T *compl,
		     char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ipRouteDest(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    static struct in_addr ipaddr;

    /* The unused component must represent a valid IP address */
    ipaddr = frungulate(4, compl);

    return (unsigned char *)(&ipaddr);
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_ipRouteNextHop(OIDC_T lastmatch,
		   int compc,
		   OIDC_T *compl,
		   char *cookie,
		   char *cp,
		   int length,
		   SNMP_PKT_T *pktp,
		   int index)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_ipRouteNextHop(lastmatch, compc, compl, cookie, cp, length, pktp, index)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	char *cp;
	int length;
	SNMP_PKT_T *pktp;
	int index;
#endif  /* NO_PP */
{
struct in_addr	dest_inaddr;
struct in_addr  gw_inaddr;
struct rte *	rtep;
short		rflags;
struct rtentry	rte;
int		rcode;

/*PRNTF0("In set iproutenexthop\n");*/

dest_inaddr = frungulate(compc, compl);

gw_inaddr.S_un.S_addr = 0L;
if (length > 0)
   {
   gw_inaddr.S_un.S_un_b.s_b1 = cp[0];
   if (length > 1)
      {
      gw_inaddr.S_un.S_un_b.s_b2 = cp[1];
      if (length > 2)
	 {
	 gw_inaddr.S_un.S_un_b.s_b3 = cp[2];
	 if (length > 3)
     	    gw_inaddr.S_un.S_un_b.s_b4 = cp[3];
	 } 
      }
   }

#if defined(DEBUG)
   {
   printf("Adding/changing a route entry...");
   printf("dest inaddr is %08.8X GW inaddr is %08.8X\n",
	  dest_inaddr.S_un.S_addr, gw_inaddr.S_un.S_addr);
   fflush(stdout);
   }
#endif

/* Is the host portion zero? */
if ((dest_inaddr.S_un.S_addr & ~route_mask(dest_inaddr)) == 0)
   then { /* The host part is zero, so we'll assume it's a "gateway" route */
#if defined(DEBUG)
	PRNTF0("GATEWAY\n");
#endif
        rflags = RTF_GATEWAY | RTF_UP;
	}
   else { /* The host part is non-zero, so we'll assume it's a "host" route */
#if defined(DEBUG)
	PRNTF0("HOST\n");
#endif
	rflags = RTF_HOST | RTF_UP;
	}

/* Does a route already exist? */
if ((rtep = ip_rte_lookup(dest_inaddr)) != (struct rte *)0)
   then { /* Yes, blow it away */
#if defined(DEBUG)
	PRNTF0("Zapping old route\n");
#endif
	(void) zap_route(dest_inaddr, rtep);
	}

/* JFN set route */
(void) memset(&rte, 0, sizeof(struct rtentry));
rte.rt_flags = rflags;
rte.rt_dst.sa_family = AF_INET;
((struct sockaddr_in *)&rte.rt_dst)->sin_addr = dest_inaddr;
rte.rt_gateway.sa_family = AF_INET;
((struct sockaddr_in *)&rte.rt_gateway)->sin_addr = gw_inaddr;

invalidate_iprte();

rcode = ioctl(snmp_socket, SIOCADDRT, &rte);
#if defined(DEBUG)
PRNTF2("ioctl returned with %d, errno is %d\n", rcode, errno);
if (rcode != 0) PERROR("This is what errno means:");
#endif
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ipRouteIfIndex(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ipRouteIfIndex(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct in_addr ipaddr;
    struct rte *rt;

    ipaddr = frungulate(4, compl);
    if ((rt = ip_rte_lookup(ipaddr)) == 0)
      return 0;
    return get_interface_number(rt->ipr->ipRouteDest);
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ipRouteNextHop(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ipRouteNextHop(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct in_addr ipaddr;
    struct rte *rt;

    ipaddr = frungulate(4, compl);
    if ((rt = ip_rte_lookup(ipaddr)) == 0)
      return 0;
    return (unsigned char *)(&rt->ipr->ipRouteNextHop);
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ipRouteType(OIDC_T lastmatch,
		     int compc,
		     OIDC_T *compl,
		     char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ipRouteType(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct in_addr ipaddr;
    struct rte *rt;

    ipaddr = frungulate(4, compl);
    if ((rt = ip_rte_lookup(ipaddr)) == 0)
      return 0;
    /* remote(4) : direct(3) */
    return rt->ipr->ipRouteType;
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_ipRouteType(OIDC_T lastmatch, int compc,
			OIDC_T *compl,
			char *cookie, INT_32_T value)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_ipRouteType(lastmatch, compc, compl, cookie, value)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	INT_32_T value;
#endif  /* NO_PP */
{
if (value == 2)	/* We only have to do anything if the new value is "invalid" */
  {
  struct in_addr ipaddr;
  struct rte *rt;

#if defined(DEBUG)
     {
     printf("Looking to delete route destination %d.%d.%d.%d\n",
	    compl[0], compl[1], compl[2], compl[3]);
     fflush(stdout);
     }
#endif
  ipaddr = frungulate(4, compl);
  if ((rt = ip_rte_lookup(ipaddr)) == 0)
      return;

#if defined(DEBUG)
  PRNTF0("found entry\n");
#endif
  (void) zap_route(ipaddr, rt);
  }
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ipRouteProto(OIDC_T lastmatch,
		      int compc,
		      OIDC_T *compl,
		      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ipRouteProto(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct in_addr ipaddr;
    struct rte *rt;

    ipaddr = frungulate(4, compl);
    if ((rt = ip_rte_lookup(ipaddr)) == 0)
      return 0;
    /* icmp(4) : local(2) */
	/*
	 * The value of ipRouteProto is obtained using the streams drivers.
	 * This value is never changed when the tables are built.
	 *
	 * It is observed that ipRouteProto is always returned as -1 by the 
	 * streams drivers. 
	 * Since -1 is not a valid enumeration, a value 1 will be returned by
	 * the following code if ipRouteProto is out of bounds
	 *
	 * From RFC 1213, 0 < ipRouteProto < 15
	 */

	return ( (rt->ipr->ipRouteProto > 0) && (rt->ipr->ipRouteProto < 15) )
		? (rt->ipr->ipRouteProto) : 1;

}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ipRouteMask(OIDC_T lastmatch,
		      int compc,
		      OIDC_T *compl,
		      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ipRouteMask(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
struct in_addr ipaddr;
struct rte *rt;

ipaddr = frungulate(4, compl);
if ((rt = ip_rte_lookup(ipaddr)) == 0)
   return 0;
return (unsigned char *)&rt->ipr->ipRouteMask;
}

#if !defined(NO_PP)
/*ARGSUSED*/
OBJ_ID_T *
  get_ipRouteInfo(OIDC_T lastmatch,
		   int compc,
		   OIDC_T *compl,
		   char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
OBJ_ID_T *
  get_ipRouteInfo(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
static OIDC_T none[] = {0, 0};
static OBJ_ID_T no_oid = { sizeof(none)/sizeof(OIDC_T), none};

/****>>>NEEDS SOME CODE HERE****/
/* JFN no support fo iproute info */
return &no_oid;
}

/****************************************************************************
NAME:  test_ipRouteEntry

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
test_ipRouteEntry(form, last_match, compc, compl, cookie, pktp, indx)
int	form;		/* See TEST_SET & TEST_GET in mib.h	*/
OIDC_T	last_match;	/* Last component matched */
int	compc;
OIDC_T	*compl;
char	*cookie;
SNMP_PKT_T *pktp;
int	indx;
{
    struct in_addr	ipaddr;

    /* There must be exactly 4 unused components */
    if (compc != INSTANCE_LEN)
      return -1;

    if (last_match > 11)
      return -1;

    read_routing();

    /* The unused components must represent a valid IP address */
    ipaddr = frungulate(4, compl);

#if 0
/*  This hunk of code is left only to be re-hacked when we figure out */
/*  how to use SNMP to add a new row to the routing table.            */
if (form == TEST_SET)
   then {
        switch (last_match)
	   {
	   case 1:	/* ipRouteDest */
	      {
	      VB_T *vbp;
	      VB_T *other_vbp;
	      int loc_indx;

	      vbp = index_to_vbp(pktp, indx);

	      /* Are we trying to set ipRouteDest?		*/
	      /* If so we need to verify that there is a	*/
	      /* corresponding set of ipRouteNextHop		*/

PRNTF0("testing for ipRouteDest\n");
	      /* Make sure the value given matches the instance... */
	      if (memcmp((char *)&ipaddr, vbp->value_u.v_network_address,
		       4) != 0) then return -2;

	      /* See if we can find a next hop with the right	*/
	      /* instance value.				*/
	      for(;;)
		 {
PRNTF0("Scanning for locator of ipRouteNextHop...");
	         if ((loc_indx = scan_vb_for_locator(pktp, 0, NEXT_HOP_LOCATOR)) == -1)
		    then {
PRNTF0("No locator\n");
			 return -2;
			 }
		 other_vbp = index_to_vbp(pktp, loc_indx);
PRNTF0("Found...");
		 if (oidcmp(compc, compl,
			other_vbp->vb_ml.ml_remaining_objid.num_components,
			other_vbp->vb_ml.ml_remaining_objid.component_list)
		     != 0)
		    then {
PRNTF0("instance matches\n");
			 vbp->vb_link = other_vbp;
			 other_vbp->vb_flags |= VFLAG_ALREADY_TEST;
			 return 0;
		         }
	         }
	      /*NOTREACHED*/
              }

	   case 7:	/* ipRouteNextHop */
	      /* Are we trying to set ipRouteNextHop?		*/
	      /* If so we can imply the IPRouteDest value from	*/
	      /* the instance.					*/
	      return 0;

	   }
        }
#endif /* 0 */

#if defined(DEBUG)
    pr_iprte();
#endif
    if (ip_rte_lookup(ipaddr))
      return 0;
    return -1;
}

/****************************************************************************
NAME:  next_ipRouteEntry

PURPOSE:  Locate the "next" object in the IP Routing table

PARAMETERS:
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.
			The following values give an example:
				1 - ipRouteDest
				2 - ipRouteIfIndex
				3 - ipRouteMetric1
				4 - ipRouteMetric2
				5 - ipRouteMetric3
				6 - ipRouteMetric4
				7 - ipRouteNextHop
				8 - ipRouteType
				9 - ipRouteProto
				10 - ipRouteAge
				11 - ipRouteMask

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
next_ipRouteEntry(last_match, tcount, tlist, rlist, cookie)
OIDC_T	last_match;	/* Last component matched */
int	tcount;
OIDC_T	*tlist;
OIDC_T	*rlist;
char	*cookie;
{
    struct rte *	rtep;
    struct rte *	best;
    int		bra[INSTANCE_LEN], lra[INSTANCE_LEN], pra[INSTANCE_LEN];
    int		i, j;
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

    read_routing();

    /* Now find the lowest value larger than the instance part */
    best = 0;
    for (rtep = &route_table; rtep != (struct rte *)0; rtep = rtep->next)
       {
       in.s_addr = rtep->ipr->ipRouteDest;
       ip_to_rlist(in,(OIDC_T *)pra);
		    
       if (objidcmp(pra, lra, INSTANCE_LEN) <= 0)
	  then continue;
       if (!best || (objidcmp(pra, bra, INSTANCE_LEN) < 0))
	  then {
	        best = rtep;
	        for (j = 0; j < INSTANCE_LEN; j++)
		   bra[j] = pra[j];
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
