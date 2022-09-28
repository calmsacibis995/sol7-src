/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)if.c	2.22 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)if.c	2.22 96/07/23 Sun Microsystems";
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
/***#define DEBUG****/
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
 * $Log:   E:/SNMPV2/AGENT/SUN/IF.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:22
 * Initial revision.
 * 
*/

#include <stdio.h>
#if (!defined(NO_STDLIB))
#include <stdlib.h>
#endif
#include <string.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/time.h>
#include  <sys/types.h>
#include  <sys/protosw.h>
#include  <sys/socket.h>
#include  <net/route.h>
#include  <netinet/in.h>
#include  <netinet/in_systm.h>
#include  <netinet/in_pcb.h>
#include  <netinet/ip.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#include  <sys/ioctl.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define	then

#define CACHE_LIFETIME		45
#define invalidate_ifcache()	if_cache_time = (time_t)0

extern unsigned long ip_net_mask();
static struct kernel_symbol ifnet = { "_ifnet", 0, 0 };
static time_t	if_cache_time;	/* When IF data last updated */
struct lif *lif;		/* Local interface list */
int ifnumber;			/* Number of interfaces */

/* Do the initial read of the interface table and makeup local table. */
int
  if_init()
{
    struct ifnet *kifp;

    invalidate_ifcache();

    if (find_loc(&ifnet) == 0)
      return -1;

    lif = LIF_NULL;
    ifnumber = 0;
    read_bytes((off_t)ifnet.offset, (char *)&kifp, sizeof(kifp));

    get_if_info();
    return 0;
}

#if !defined(NO_PP)
void
get_if_info(void)
#else   /* NO_PP */
void
get_if_info()
#endif  /* NO_PP */
{
struct ifnet *kifp;

if ((cache_now - if_cache_time) <= CACHE_LIFETIME)
  return;
if_cache_time = cache_now;

if (lif != LIF_NULL)	/* Free up old data */
   {
   struct lif *lifp;
   struct lif *nxtlifp;
   for (lifp = lif; lifp;)
      {
      struct ifaddr *ap;
      nxtlifp = lifp->next;

/* Clean up any old list of addresses */
      for (ap = lifp->ifp->if_addrlist; ap != (struct ifaddr *)0;)
	 {
	 struct ifaddr *next_ap;
	 next_ap = ap->ifa_next;
	 (void) free((char *)ap);
	 ap = next_ap;
	 }
      (void) free(lifp->ifp);
      (void) free(lifp);
      lifp = nxtlifp;
      }
   }

ifnumber = 0;
lif = LIF_NULL;
read_bytes((off_t)ifnet.offset, (char *)&kifp, sizeof(kifp));

while (kifp)
   {
   char inum_buf[16];
   struct lif *lp;
   struct ifnet *new_ifp;
   int i;

   if ((lp = (struct lif *)malloc(sizeof(struct lif))) == 0)
      return;
   (void)memset((char *)lp, 0, sizeof(struct lif));

   if ((new_ifp = (struct ifnet *)malloc(sizeof(struct ifnet))) == 0)
      return;
   (void)memset((char *)(new_ifp), 0, sizeof(struct ifnet));
   read_bytes((off_t)kifp, (char *)(new_ifp), sizeof(struct ifnet));

   lp->next = lif;
   lif = lp;
   lp->koff = (off_t)kifp;
   lp->ifp = new_ifp;
   new_ifp->if_addrlist = (struct ifaddr *)0;

   read_bytes((off_t)(new_ifp->if_name), lp->name, IFNAMSIZ);

   /* Not everybody has itoa(), oh well, gotta use sprintf() */
   sprintf(inum_buf, "%u", new_ifp->if_unit);
   (void)strcat(lp->name, inum_buf);

   read_if(lp);	/* Read the stuff that changes */

   if ((new_ifp->if_flags) & IFF_LOOPBACK)
      {
      lp->type = 24;		/* software loopback */
      lp->speed = 10000000;
      }

   else if ((strncmp(lp->name, "ie", 2) == 0) || /* Intel 82586 */
	    (strncmp(lp->name, "le", 2) == 0) || /* AMD LANCE */
	    (strncmp(lp->name, "ec", 2) == 0))   /* 3-COM */
      {
      struct arpcom arpcom;
      lp->type = 6;		/* ethernet */
      lp->speed = 10000000;
      lp->flags |= LIF_HAS_PHYSADDR;
      /* The following gets the Ethernet address.		*/
      read_bytes((off_t)(lp->koff) +
		 (off_t)((char *)&arpcom.ac_enaddr - (char *)&arpcom),
		 (char *)&(lp->ac_enaddr),
		 sizeof(struct ether_addr));
      }
   else if ((strncmp(lp->name, "fd", 2) == 0) ||  /* fddi interface */
   			(strncmp(lp->name, "nf", 2) == 0) ||  /* fddi interface */
            (strncmp(lp->name, "bf", 2) == 0))  /* fddi interface */
      {  
      lp->type = 15;
      lp->speed = 100000000;
      }  
   else if (strncmp(lp->name, "tr", 2) == 0)  /* token ring interface */
      {  
      lp->type = 9;
      lp->speed = 16000000;       /* speed either 16000000 or 4000000 */
      }
   else if (strncmp(lp->name, "be", 2) == 0)  /* bigmac interface */
      {
      lp->type = 33;
      lp->speed = 1000000000;
      }
    else if (strncmp(lp->name, "qe", 2) == 0)  /* qed interface */
      {
      lp->type = 34;
      lp->speed = 1000000000;
      }
   else
      {
        for( i=0; i< new_device_pointer; i++) {
                if(strcmp(lp->name, new_devices[i].name))
                        break;
        }
 
        if( i < new_device_pointer) {
                lp->type = new_devices[i].type;
                lp->speed = new_devices[i].speed;
        } else {
                lp->type = 1;           /* other */
                lp->speed = 9600;
        }
      }

   kifp = new_ifp->if_next;
   ifnumber++;
   }
}


/* Update an interface's information if it's too old. */
#if !defined(NO_PP)
void
  read_if(struct lif *lp)
#else   /* NO_PP */
void
  read_if(lp)
	struct lif *lp;
#endif  /* NO_PP */
{
struct ifaddr *ap;
struct ifaddr *kap;
struct ifaddr **aptr;
struct ifreq  if_req;

/* Clean up any old list of addresses */
for (ap = lp->ifp->if_addrlist; ap != (struct ifaddr *)0;)
   {
   struct ifaddr *next_ap;
   next_ap = ap->ifa_next;
   (void) free((char *)ap);
   ap = next_ap;
   }
lp->ifp->if_addrlist = (struct ifaddr *)0;

read_bytes((off_t)lp->koff, (char *)(lp->ifp), sizeof(struct ifnet));

lp->status = ((lp->ifp->if_flags) & IFF_UP) ? 1 : 2;

/* Get the current subnet mask */
(void)strncpy(if_req.ifr_name, lp->name, IFNAMSIZ);
(void)ioctl(snmp_socket, SIOCGIFNETMASK, &if_req);

lp->netmask.s_addr = ((struct sockaddr_in *)&if_req.ifr_addr)->sin_addr.s_addr;

/* Build the address list */
if (lp->ifp->if_addrlist != (struct ifaddr *)0)
   then {
        for (aptr = &(lp->ifp->if_addrlist), kap = lp->ifp->if_addrlist;
	     kap != (struct ifaddr *)0;)
	   {
	   off_t kernap;
	   ap = (struct ifaddr *)malloc(sizeof(struct ifaddr));
	   *aptr = ap;	/* Update the link in the previous entry */
	   aptr = &(ap->ifa_next);
	   kernap = (off_t)kap;
	   read_bytes((off_t)kernap, (char *)ap, sizeof(struct ifaddr));
#if 0
#ifdef DEBUG
	   {
	   printf("Address %s, dst addr %s, ifp=%8.8X nxt=%8.8X\n",
		  (char *)inet_ntoa(((struct sockaddr_in *)&(ap->ifa_addr))->sin_addr.s_addr),
		  (char *)inet_ntoa(((struct sockaddr_in *)&(ap->ifa_ifu.ifu_dstaddr))->sin_addr.s_addr),
		  ap->ifa_ifp, ap->ifa_next);
	   fflush(stdout);
	   }
#endif	/* DEBUG */
#endif	/* 0 */
	   kap = ap->ifa_next;
	   ap->ifa_next = (struct ifaddr *)0;
	   }
	}
   else { /* No address! Build a dummy entry */
        lp->flags |= LIF_NO_ADDRESS;
        lp->ifp->if_addrlist = (struct ifaddr *)malloc(sizeof(struct ifaddr));
	memset((char *)(&(lp->ifp->if_addrlist)), 0, sizeof(struct ifaddr));
        }
}

#if !defined(NO_PP)
struct lif *
getlifbyip(struct in_addr *iap)
#else   /* NO_PP */
struct lif *
getlifbyip(iap)
	struct in_addr *iap;
#endif  /* NO_PP */
{
struct lif *lifp;
for (lifp = lif; lifp; lifp = lifp->next)
   {
   if (lifp->flags & LIF_NO_ADDRESS) then continue;
   if (iap->s_addr ==
       ((struct sockaddr_in *)&(lifp->ifp->if_addrlist->ifa_addr))->
	    sin_addr.s_addr)
      then return lifp;
   }
return LIF_NULL;
}

#if !defined(NO_PP)
struct lif *
getlifbyip_netmask(struct in_addr ia)
#else /* NO_PP */
struct lif *
getlifbyip_netmask(ia)
     struct in_addr ia;
#endif /* NO_PP */
{
  struct lif *lifp;
  unsigned long netmask;
  unsigned long netaddr;
  int ifno = 0;

  for (lifp = lif; lifp; lifp = lifp->next) {
    ifno++;
    if (lifp->flags & LIF_NO_ADDRESS)
      continue;
    netaddr = ((struct sockaddr_in *)&(lifp->ifp->if_addrlist->ifa_addr))->
      sin_addr.s_addr;
    if (lifp->netmask.s_addr && (ia.s_addr & lifp->netmask.s_addr) ==
	(netaddr & lifp->netmask.s_addr))
      return ifno;
  }
  return 2;
}
  
#if !defined(NO_PP)
int
getlifindex(struct lif *usrp)
#else   /* NO_PP */
int
getlifindex(usrp)
	struct lif *usrp;
#endif  /* NO_PP */
{
struct lif *lifp;
int i;
for (lifp = lif, i = 1; lifp; lifp = lifp->next, i++)
   {
   if (lifp == usrp)
      then return i;
   }

return 0;
}

#if !defined(NO_PP)
struct lif *
getlifbycompl(int compc, OIDC_T *compl)
#else   /* NO_PP */
struct lif *
getlifbycompl(compc, compl)
   int compc;
   OIDC_T *compl;
#endif  /* NO_PP */
{
struct in_addr ipaddr;
ipaddr = frungulate(4, compl);
return getlifbyip(&ipaddr);
}


#if !defined(NO_PP)
int
  get_interface_number(off_t kifp)
#else   /* NO_PP */
int
  get_interface_number(kifp)
	off_t kifp;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = 1; lifp; i++, lifp = lifp->next) {
	if (lifp->koff == kifp)
	  return i;
    }
#define IFDEBUG
#if defined(IFDEBUG)
{
struct lif *dlifp;
   {
   printf("Can not find interface number: looking for %8.8X\n", kifp);
   printf("Known values are:\n");
   for (dlifp = lif; dlifp; dlifp = dlifp->next)
      printf("     %8.8X\n", dlifp->koff);
   fflush(stdout);
   }
}
#endif /* IFDEBUG */
    return 2; /* If we can't find it, then say it's the 1st LAN */
}    

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifNumber(OIDC_T lastmatch,
		  int compc,
		  OIDC_T *compl,
		  char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifNumber(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    get_if_info();
    return (INT_32_T)ifnumber;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifIndex(OIDC_T lastmatch,
		  int compc,
		  OIDC_T *compl,
		  char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifIndex(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    return (INT_32_T)(compl[0]);
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ifDescr(OIDC_T lastmatch,
		 int compc,
		 OIDC_T *compl,
		 char *cookie,
		 int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ifDescr(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0) {
	/* This shouldn't happen because the interface was checked by the
	 * test function already. */
	*lengthp = 10;
	return (unsigned char *)"SNMP Error";
    }

    *lengthp = strlen(lifp->name);
    return (unsigned char *)lifp->name;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifType(OIDC_T lastmatch,
		  int compc,
		  OIDC_T *compl,
		  char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifType(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 1;		/* other */

    return lifp->type;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifMtu(OIDC_T lastmatch,
		  int compc,
		  OIDC_T *compl,
		  char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifMtu(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_mtu;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifSpeed(OIDC_T lastmatch,
		 int compc,
		 OIDC_T *compl,
		 char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifSpeed(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->speed;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
  get_ifPhysAddress(OIDC_T lastmatch,
		    int compc,
		    OIDC_T *compl,
		    char *cookie,
		    int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_ifPhysAddress(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0) {
	/* This shouldn't happen because the interface was checked by the
	 * test function already. */
	*lengthp = 0;
	return 0;
    }

    if (lifp->flags & LIF_HAS_PHYSADDR)
       then {
	    *lengthp = sizeof(struct ether_addr);
	    return (unsigned char *)&(lifp->ac_enaddr);
            }

    *lengthp = 0;
    return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifAdminStatus(OIDC_T lastmatch,
		       int compc,
		       OIDC_T *compl,
		       char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifAdminStatus(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->status;
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_ifAdminStatus(OIDC_T lastmatch, int compc,
			OIDC_T *compl,
			char *cookie, INT_32_T value)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_ifAdminStatus(lastmatch, compc, compl, cookie, value)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	INT_32_T value;
#endif  /* NO_PP */
{
struct lif *lifp;
int i;
struct ifreq  if_req;
struct timeval now_is;
unsigned long tticks_now;

for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
if (lifp == 0)
   /* This shouldn't happen because the interface was checked by the test
    * function already. */
   return;

(void)strncpy(if_req.ifr_name, lifp->name, IFNAMSIZ);
(void)ioctl(snmp_socket, SIOCGIFFLAGS, &if_req);

(void) gettimeofday(&now_is, (struct timezone *)0);
tticks_now = (unsigned long)(((now_is.tv_sec - boot_at.tv_sec) * 100) +
			     (now_is.tv_usec / 10000));

switch(value)
   {
   case 1:	/* Turn interface on */
	   lifp->ifp->if_flags |= IFF_UP;
	   invalidate_ifcache();
	   if (if_req.ifr_flags & IFF_UP) return; /* Already up */
	   if_req.ifr_flags |= IFF_UP;
	   lifp->change_time = tticks_now;
	   (void)ioctl(snmp_socket, SIOCSIFFLAGS, &if_req);
	   send_traps(snmp_socket, LINK_UP, compl[0]);
	   break;
   case 2:	/* Turn interface off */
   case 3:	/* Turn interface to "test mode" */
	   lifp->ifp->if_flags &= ~IFF_UP;
	   invalidate_ifcache();
	   if (!(if_req.ifr_flags & IFF_UP)) return; /* Already down */
	   send_traps(snmp_socket, LINK_DOWN, compl[0]);
	   if_req.ifr_flags &= ~IFF_UP;
	   lifp->change_time = tticks_now;
	   (void)ioctl(snmp_socket, SIOCSIFFLAGS, &if_req);
	   break;
   }
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
  get_ifOperStatus(OIDC_T lastmatch,
		      int compc,
		      OIDC_T *compl,
		      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_ifOperStatus(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_flags & IFF_UP ? 1 : 2;
}

/* The OS doesn't keep track of when interface states change.	*/
/* The best we can do is to track when SNMP change the state.	*/
#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifLastChange(OIDC_T lastmatch,
		      int compc,
		      OIDC_T *compl,
		      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifLastChange(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->change_time;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifInUcastPkts(OIDC_T lastmatch,
		       int compc,
		       OIDC_T *compl,
		       char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifInUcastPkts(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_ipackets;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifInErrors(OIDC_T lastmatch,
		    int compc,
		    OIDC_T *compl,
		    char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifInErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_ierrors;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutUcastPkts(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifOutUcastPkts(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_opackets;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutErrors(OIDC_T lastmatch,
		     int compc,
		     OIDC_T *compl,
		     char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifOutErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_oerrors + lifp->ifp->if_collisions;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutQLen(OIDC_T lastmatch,
		   int compc,
		   OIDC_T *compl,
		   char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifOutQLen(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return 0;

    return lifp->ifp->if_snd.ifq_len;
}

#if !defined(NO_PP)
/*ARGSUSED*/
OBJ_ID_T *
  get_ifSpecific(OIDC_T lastmatch,
		   int compc,
		   OIDC_T *compl,
		   char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
OBJ_ID_T *
  get_ifSpecific(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;
    static OIDC_T none[] = {0, 0};
    static OBJ_ID_T no_oid = { sizeof(none)/sizeof(OIDC_T), none};

    for (lifp = lif, i = compl[0]; lifp && --i; lifp = lifp->next)
      ;
    if (lifp == 0)
      /* This shouldn't happen because the interface was checked by the test
       * function already. */
      return &no_oid;

    return &no_oid;
}

/****************************************************************************
NAME:  iftable_test

PURPOSE:  Test whether a given object exists in the Interface table

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
iftable_test(form, last_match, compc, compl, cookie)
	int	form;	/* See TEST_SET & TEST_GET in mib.h	*/
	OIDC_T	last_match; /* Last component matched */
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
{
    struct lif *lifp;
    int i;

    /* There must be exactly 1 unused component */
    if (compc != 1)
      return -1;

    if (last_match > 22)
      return -1;

    get_if_info();

    if ((compl[0] < 1) || (compl[0] > ifnumber))
      return -1;

    return 0;
}

/****************************************************************************
NAME:  iftable_next

PURPOSE:  Locate the "next" object in the Interface table

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
iftable_next(last_match, tcount, tlist, rlist, cookie)
	OIDC_T	last_match; /* Last component matched */
	int	tcount;
	OIDC_T	*tlist;
	OIDC_T	*rlist;
	char	*cookie;
{
get_if_info();
if (tcount == 0)
  {
  rlist[0] = 1;
  return 1;
  }

if (tlist[0] < ifnumber)
  {
  struct lif *lifp;
  int i;

  rlist[0] = tlist[0] + 1;
  for (lifp = lif, i = rlist[0]; lifp && --i; lifp = lifp->next)
      ;
  if (lifp == 0) return 0;

  return 1;
  }

/* Pass on it */
return 0;
}

/* this routine has been added because of an incompatibility between
 * snmp mib i and snmp mib ii.
 * Basically, the attribute ifAdminStatus is a enum under mib ii.
 * whereas doesn't appear so under mib i. For any value other
 * than the enumeration, snmpd would silently ignore giving the impression
 * that the user specified value  has been accepted. With this routine this
 * would no longer be the case and an appropriate error would be reported.
 * (bad value)
 * granga  12/19/94  bug - 1185563 
 */

int
test_ifAdminStatus(form, last_match, compc, compl, cookie, pktp, index)
        int     form;   /* See TEST_SET & TEST_GET in mib.h     */
        OIDC_T  last_match; /* Last component matched */
        int     compc;
        OIDC_T  *compl;
        char    *cookie;
	SNMP_PKT_T *pktp;
	int 	index;
{
	VB_T    *vbp;
	int i;
	int retval;

	retval = iftable_test(form, last_match, compc, compl, cookie);
	
	if (retval < 0)
		return(retval);

	if (form != TEST_SET)
		return (0);

	vbp = pktp->pdu.std_pdu.std_vbl.vblist;
	for (i =0; i < index ; i++)
		vbp++;

	switch (vbp->value_u.v_number) {

		case  1:
		case  2:
		case  3:
			return 0;

		default:
			return (-3);
	}
	

}
