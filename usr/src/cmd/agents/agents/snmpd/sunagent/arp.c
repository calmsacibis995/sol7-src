/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)arp.c	2.17 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)arp.c	2.17 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/ARP.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:18
 * Initial revision.
 * 
*/

#include <stdio.h>
#include <string.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/types.h>
#include  <sys/protosw.h>
#include  <sys/socket.h>
#include  <net/route.h>
#include  <netinet/in.h>
#include  <netinet/in_systm.h>
#include  <netinet/ip.h>
#include  <sys/sockio.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
struct	os_41_arptab {
	struct	in_addr at41_iaddr;	/* internet address */
	union {
	    struct ether_addr atu41_enaddr;	/* ethernet address */
	    long   atu41_tvsec;			/* timestamp if incomplete */
	} 	at41_union;
	u_char	at41_timer;		/* minutes since last reference */
	u_char	at41_flags;		/* flags */
	struct	mbuf *at41_hold;	/* last packet until resolved/timeout */
};
# define at41_enaddr at41_union.atu41_enaddr

/* Sun OS 4.0 had a slightly different format ...
 * Internet to ethernet address resolution table.
 */
struct	os_40_arptab {
	struct	in_addr at40_iaddr;	/* internet address */
	struct	ether_addr at40_enaddr;	/* ethernet address */
	u_char	at40_timer;		/* minutes since last reference */
	u_char	at40_flags;		/* flags */
	struct	mbuf *at40_hold;	/* last packet until resolved/timeout */
};

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define then

#define CACHE_LIFETIME		45

#define AT_INSTANCE_LEN	6
#define AT_INTERFACE	0
#define AT_PROT		1
#define AT_ADDR		2

#define N2M_INSTANCE_LEN	5
#define N2M_INTERFACE		0
#define N2M_ADDR		1

static struct kernel_symbol arptab = { "_arptab", 0, 0 };
static struct kernel_symbol arptab_size = { "_arptab_size", 0, 0 };

/* the "digested" arp table is held as follows: */
typedef struct atab_s
	{
	struct	in_addr    a_iaddr;	/* internet address */
	struct  ether_addr a_enaddr;	/* ethernet address */
	u_char		   a_flags;	/* flags */
	int		   a_ifnum;	/* interface number */
	} atab_t;
static atab_t *atabp = (atab_t *)0;
static atab_cnt = 0;	/* Number of useful entries referenced by atabp */

/* For a_flags above: */
#define	ATA_HAS_MBUF	0x01

static struct os_41_arptab *krnl_atab = (struct os_41_arptab *)0;
static int krnl_atab_cnt = 0;	/* # of items the kernel maintains  */
static int krnl_atab_bytes;	/* # bytes referenced by krnl_atab  */

#define	ARP_FORCE	1
#define invalidate_arptab() arp_cache_time = (time_t)0
static time_t arp_cache_time = 0;

#ifdef DEBUG
static
void
pr_arptab()
{
atab_t * tab;
int i;

printf("Arp table has %d entries:\n", atab_cnt);
if (atab_cnt != 0)
   then {
        for (tab = atabp, i = 0; i < atab_cnt; tab++, i++)
	   {
	   printf("%3d  %15s  %02X:%02X:%02X:%02X:%02X:%02X  %2.2X  %3d\n",
		  i, inet_ntoa(tab->a_iaddr),
		  tab->a_enaddr.ether_addr_octet[0],
		  tab->a_enaddr.ether_addr_octet[1],
		  tab->a_enaddr.ether_addr_octet[2],
		  tab->a_enaddr.ether_addr_octet[3],
		  tab->a_enaddr.ether_addr_octet[4],
		  tab->a_enaddr.ether_addr_octet[5],
		  tab->a_flags, tab->a_ifnum);
           }
        }
}
#endif

#if !defined(NO_PP)
static int
read_arptab(void)
#else   /* NO_PP */
static int
read_arptab()
#endif  /* NO_PP */
{
atab_t *atp;

/* Check whether we actually need to read the cache */
if ((cache_now - arp_cache_time) <= CACHE_LIFETIME)
   then return 0;

arp_cache_time = cache_now;
if (find_loc(&arptab) == 0)
   then return -1;

/* The following assumes that the Sun OS4.1 struct arptab is	*/
/* larger than the OS 4.0 version.				*/
(void) read_bytes((off_t)arptab.offset, (char *)krnl_atab,
		  krnl_atab_bytes);

/* Now squeeze out the useless entries */
/* Have to do the following two ways, because Sun OS 4.0 had a	*/
/* different format table.  (There is an implicit long alignment*/
/* in the Sun OS 4.1 version.)					*/
if (sun_os_ver == 40)
   then { /* Sun OS 4.0 */
        int i;
        struct os_40_arptab *kp;
        for (i = 0, atp = atabp, atab_cnt = 0,
	     kp = (struct os_40_arptab *)krnl_atab;
	     i < krnl_atab_cnt;
	     i++, kp++)
	   {
	   if (!(kp->at40_flags & ATF_COM)) continue;
	   atp->a_iaddr = kp->at40_iaddr;
	   atp->a_enaddr = kp->at40_enaddr;
	   /* Make note whether an MBUF is attached */
	   atp->a_flags = kp->at40_hold != (struct mbuf *)0 ? ATA_HAS_MBUF : 0;
	   atp->a_ifnum = getlifbyip_netmask(atp->a_iaddr);
	   atp++;
	   atab_cnt++;
	   }
        }
   else { /* Sun OS 4.1 */
        int i;
        struct os_41_arptab *kp;
        for (i = 0, atp = atabp, atab_cnt = 0,
	     kp = (struct os_41_arptab *)krnl_atab;
	     i < krnl_atab_cnt;
	     i++, kp++)
	   {
	   if (!(kp->at41_flags & ATF_COM)) continue;
	   atp->a_iaddr = kp->at41_iaddr;
	   atp->a_enaddr = kp->at41_enaddr;
	   /* Make note whether an MBUF is attached */
	   atp->a_flags = kp->at41_hold != (struct mbuf *)0 ? ATA_HAS_MBUF : 0;
	   atp->a_ifnum = getlifbyip_netmask(atp->a_iaddr);
	   atp++;
	   atab_cnt++;
	   }
        }
#ifdef DEBUG
pr_arptab();
#endif
return 0;
}

/*ARGSUSED*/
INT_32_T
  get_atIfIndex(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    return (INT_32_T)(compl[AT_INTERFACE]);
}

/*ARGSUSED*/
unsigned char *
  get_atPhysAddress(lastmatch, compc, compl, cookie, lengthp)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; 
int 	*lengthp;
{
    struct in_addr ipaddr;
    atab_t *tab;
    int i;

    *lengthp = 6;
    ipaddr = frungulate(4, compl+AT_ADDR);

    /* Look up entry */
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	if ((ipaddr.s_addr == tab->a_iaddr.s_addr) &&
	    (compl[AT_INTERFACE] == tab->a_ifnum))
	  return tab->a_enaddr.ether_addr_octet;
    }
    /* This can't happen because the test routine has guarenteed that
     * the entry is there and valid. */
    return atabp[0].a_enaddr.ether_addr_octet;
}

#if !defined(NO_PP)
static
void
zap_arp(struct in_addr ipaddr, int ifnum, char *cp, int length)
#else   /* NO_PP */
static
void
zap_arp(ipaddr, ifnum, cp, length)
	struct in_addr ipaddr;
	int ifnum;
	char *cp;
	int length;
#endif  /* NO_PP */
{
atab_t *tab;
int i;

  /* Look up entry */
  for (tab = atabp, i = 0; i < atab_cnt; i++, tab++)
     {
     if ((ipaddr.s_addr == tab->a_iaddr.s_addr) &&
	 (ifnum == tab->a_ifnum))
        {
	struct arpreq areq;

	/* Don't mess with anything that has a transmission pending */
	if (tab->a_flags & ATA_HAS_MBUF) return;

        (void)memset((char *)&areq, 0, sizeof(areq));
	((struct sockaddr_in *)&areq.arp_pa)->sin_family = AF_INET;
	((struct sockaddr_in *)&areq.arp_pa)->sin_addr.s_addr
		  = tab->a_iaddr.s_addr;
	areq.arp_ha.sa_family = AF_UNSPEC;

	if (length == 0)
	  {	/* Blow the entry away here */
	  (void)memcpy((char *)areq.arp_ha.sa_data,
		       (char *)(tab->a_enaddr.ether_addr_octet),
		       6);
	  (void)ioctl(snmp_socket, SIOCDARP, &areq);
          }
	else
	  {
	  (void)memcpy((char *)areq.arp_ha.sa_data,
		       cp, min(length, 6));
	  (void)ioctl(snmp_socket, SIOCSARP, &areq);
	  }
	invalidate_arptab();
	return;
        }
     }
}

/*ARGSUSED*/
#if !defined(NO_PP)
void
set_atPhysAddress(OIDC_T lastmatch, int compc,
			OIDC_T *compl,
			char *cookie, char *cp, int length)
#else   /* NO_PP */
void
set_atPhysAddress(lastmatch, compc, compl, cookie, cp, length)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	char *cp;
	int length;
#endif  /* NO_PP */
{
zap_arp(frungulate(4, compl+AT_ADDR), (int)(compl[AT_INTERFACE]), cp, length);
}

/*ARGSUSED*/
unsigned char *
  get_atNetAddress(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
    static struct in_addr ipaddr;

    ipaddr = frungulate(4, compl+AT_ADDR);
    return (unsigned char *)(&ipaddr);
}


/****************************************************************************
NAME:  arptable_test

PURPOSE:  Test whether a given object exists in the ARP table

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
  arptable_test(form, last_match, compc, compl, cookie)
int	form;		/* See TEST_SET & TEST_GET in mib.h	*/
OIDC_T	last_match;	/* Last component matched */
int	compc;
OIDC_T	*compl;
char	*cookie;
{
    struct in_addr ipaddr;
    atab_t *tab;
    int i;

    /* There must be exactly 6 unused components */
    if (compc != AT_INSTANCE_LEN)
      return -1;

    if (last_match > 3)
      return -1;

    /* Only support protocol IP in ARP table */
    if (*(compl+AT_PROT) != 1)
      return -1;

    ipaddr = frungulate(4, compl+AT_ADDR);

    if (read_arptab() == -1)
      return -1;

    /* Look up entry */
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	if ((ipaddr.s_addr == tab->a_iaddr.s_addr) &&
	    (compl[AT_INTERFACE] == tab->a_ifnum))
	  return 0;
    }
    return -1;
}

/****************************************************************************
NAME:  arptable_next

PURPOSE:  Locate the "next" object in the ARP table

PARAMETERS:
	OIDC_T		Last component of the object id leading to
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.
			The following values give an example:
				1 - atIfIndex
				2 - atPhysaddress
				3 - atNetaddress

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
  arptable_next(last_match, tcount, tlist, rlist, cookie)
OIDC_T	last_match;	/* Last component matched */
int	tcount;
OIDC_T	*tlist;
OIDC_T	*rlist;
char	*cookie;
{
    atab_t *	tab;
    atab_t *	best;
    int		bra[AT_INSTANCE_LEN], lra[AT_INSTANCE_LEN], vra[AT_INSTANCE_LEN];
    int		i;
    struct in_addr	vaddr;

    /* Read tlist into an array with each segment larger than the value can be
     * so -1 can represent a value smaller than any legal value. */
    for (i = 0; i < AT_INSTANCE_LEN; i++) {
	if (tcount) {
	    lra[i] = tlist[i];
	    tcount--;
	}
	else
	  lra[i] = -1;
    }

    if (read_arptab() == -1)
      return 0;

    /* Now find the lowest value larger than the given one */
    best = (atab_t *)0;
    vra[AT_PROT] = 1;		/* Only IP supported by ARP. */
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	vaddr.s_addr = tab->a_iaddr.s_addr;
	ip_to_rlist(vaddr, (OIDC_T *)(vra+AT_ADDR));
	vra[AT_INTERFACE] = tab->a_ifnum;
	
	if (objidcmp(vra, lra, AT_INSTANCE_LEN) <= 0) {
	    continue;
	}
	if (!best || (objidcmp(vra, bra, AT_INSTANCE_LEN) < 0)) {
	    int j;
	    best = tab;
	    for (j = 0; j < AT_INSTANCE_LEN; j++)
	      bra[j] = vra[j];
	}
    }

    if (best) {
	rlist[AT_INTERFACE] = best->a_ifnum;
	rlist[AT_PROT] = 1;	/* Only prot IP supported by ARP */
	vaddr.s_addr = best->a_iaddr.s_addr;
	ip_to_rlist(vaddr, (OIDC_T *)(rlist+AT_ADDR));
	return AT_INSTANCE_LEN;
    }

    /* Pass on it */
    return 0;
}

#if 0
poid(str, oid, len)
char *str;
int *oid;
int len;
{
    PRNTF1("%s", str);
    while (len--)
      PRNTF1("%d ", *oid++);
}
#endif

/*ARGSUSED*/
INT_32_T
  get_n2mIfIndex(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

return (INT_32_T)(compl[N2M_INTERFACE]);
}

/*ARGSUSED*/
unsigned char *
  get_n2mPhysAddress(lastmatch, compc, compl, cookie, lengthp)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie;
int 	*lengthp;
 {
    struct in_addr ipaddr;
    atab_t *tab;
    int i;

    *lengthp = 6;
    ipaddr = frungulate(4, compl+N2M_ADDR);

    /* Look up entry */
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	if ((ipaddr.s_addr == tab->a_iaddr.s_addr) &&
	    (compl[N2M_INTERFACE] == tab->a_ifnum))
	  return tab->a_enaddr.ether_addr_octet;
    }
    /* This can't happen because the test routine has guarenteed that
     * the entry is there and valid. */
    return atabp[0].a_enaddr.ether_addr_octet;
}

/*ARGSUSED*/
unsigned char *
  get_n2mNetAddress(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
    static struct in_addr ipaddr;

    ipaddr = frungulate(4, compl+N2M_ADDR);
    return (unsigned char *)(&ipaddr);
}

/****************************************************************************
NAME:  n2mtable_test

PURPOSE:  Test whether a given object exists in the ipNetToMedia table

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
  n2mtable_test(form, last_match, compc, compl, cookie)
int	form;		/* See TEST_SET & TEST_GET in mib.h	*/
OIDC_T	last_match;	/* Last component matched */
int	compc;
OIDC_T	*compl;
char	*cookie;
{
    struct in_addr ipaddr;
    atab_t *tab;
    int i;

    /* There must be exactly 5 unused components */
    if (compc != N2M_INSTANCE_LEN)
      return -1;

    if (last_match > 4)
      return -1;

    ipaddr = frungulate(4, compl+N2M_ADDR);

    if (read_arptab() == -1)
      return -1;

    /* Look up entry */
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	if ((ipaddr.s_addr == tab->a_iaddr.s_addr) &&
	    (compl[N2M_INTERFACE] == tab->a_ifnum))
	  return 0;
    }
    return -1;
}

/****************************************************************************
NAME:  n2mtable_next

PURPOSE:  Locate the "next" object in the ipNetToMedia table

PARAMETERS:
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.

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
  n2mtable_next(last_match, tcount, tlist, rlist, cookie)
OIDC_T	last_match;	/* Last component matched */
int	tcount;
OIDC_T	*tlist;
OIDC_T	*rlist;
char	*cookie;
{
    atab_t *	tab;
    atab_t *	best;
    int		bra[N2M_INSTANCE_LEN], lra[N2M_INSTANCE_LEN], vra[N2M_INSTANCE_LEN];
    int		i;
    struct in_addr	vaddr;

    /* Read tlist into an array with each segment larger than the value can be
     * so -1 can represent a value smaller than any legal value. */
    for (i = 0; i < N2M_INSTANCE_LEN; i++) {
	if (tcount) {
	    lra[i] = tlist[i];
	    tcount--;
	}
	else
	  lra[i] = -1;
    }

    if (read_arptab() == -1)
      return 0;

    /* Now find the lowest value larger than the given one */
    best = (atab_t *)0;
    for (tab = atabp, i = 0; i < atab_cnt; i++, tab++) {
	vaddr.s_addr = tab->a_iaddr.s_addr;
	vra[N2M_INTERFACE] = tab->a_ifnum;
	ip_to_rlist(vaddr, (OIDC_T *)(vra+N2M_ADDR));
	
	if (objidcmp(vra, lra, N2M_INSTANCE_LEN) <= 0) {
	    continue;
	}
	if (!best || (objidcmp(vra, bra, N2M_INSTANCE_LEN) < 0)) {
	    int j;
	    best = tab;
	    for (j = 0; j < N2M_INSTANCE_LEN; j++)
	      bra[j] = vra[j];
	}
    }

    if (best) {
	rlist[N2M_INTERFACE] = best->a_ifnum;
	vaddr.s_addr = best->a_iaddr.s_addr;
	ip_to_rlist(vaddr, (OIDC_T *)(rlist+N2M_ADDR));
	return N2M_INSTANCE_LEN;
    }

    /* Pass on it */
    return 0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_ipNetToMediaType(OIDC_T lastmatch, int compc,
			OIDC_T *compl,
			char *cookie, INT_32_T value)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_ipNetToMediaType(lastmatch, compc, compl, cookie, value)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	INT_32_T value;
#endif  /* NO_PP */
{
if (value == 2)	/* We only have to do anything if the new value is "invalid" */
  {
  zap_arp(frungulate(4, compl+N2M_ADDR), (int)(compl[N2M_INTERFACE]),
	  (char *)0, 0);
  }
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_n2mPhysAddress(OIDC_T lastmatch, int compc,
			OIDC_T *compl,
			char *cookie, char *cp, int length)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_n2mPhysAddress(lastmatch, compc, compl, cookie, cp, length)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	char *cp;
	int length;
#endif  /* NO_PP */
{
zap_arp(frungulate(4, compl+N2M_ADDR), (int)(compl[N2M_INTERFACE]), cp, length);
}

int
arp_init()
{
if (find_loc(&arptab_size) == 0) return -1;
if (find_loc(&arptab) == 0) return -1;
krnl_atab_cnt = read_int((off_t)arptab_size.offset);
#ifdef DEBUG
PRNTF1("Kernel maintains %d ARP entries\n", krnl_atab_cnt);
#endif
if (sun_os_ver == 40)
   then { /* Sun OS 4.0 */
        krnl_atab_bytes = krnl_atab_cnt * sizeof(struct os_40_arptab);
	}
   else { /* Sun OS 4.1 */
        krnl_atab_bytes = krnl_atab_cnt * sizeof(struct os_41_arptab);
	}
if ((krnl_atab = (struct os_41_arptab *)malloc(krnl_atab_bytes))
    == (struct os_41_arptab *)0)
   then return -1;

if ((atabp = (atab_t *)malloc(krnl_atab_cnt * sizeof(atab_t)))
    == (atab_t *)0)
   then {
	free((char *)krnl_atab);
	return -1;
	}

/*return read_arptab();*/
}
