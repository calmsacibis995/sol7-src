/* Copyright 1988 - 02/25/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)if-svr4.c	1.30 97/02/25 Sun Microsystems"
#else
static char sccsid[] = "@(#)if-svr4.c	1.30 97/02/25 Sun Microsystems";
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
#include  <sys/sockio.h>
#include <inet/common.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <sys/dlpi.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <kstat.h>


#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define	then

#define CACHE_LIFETIME		45
#define invalidate_ifcache()	if_cache_time = (time_t)0
void get_physical_addr();
void update_counters();

static time_t	if_cache_time;	/* When IF data last updated */
struct lif lif_head;		/* Local interface list */
struct lif *lif;		/* Local interface list */
mib_item_t *item_if = NULL;
int ifnumber;			/* Number of interfaces */
struct strioctl tmpstrioctl;

void
pr_if()
{
  struct lif *lifp;
  int i,j;

  printf("---- if ----\n");
  printf("#  Device    etheraddr    speed\n");
  lifp = lif;
  for (i=0;i<ifnumber;i++)
    {
	printf("%d   %s\t",lifp->koff,lifp->name);
	for (j=0;j<6;j++)
		printf("%x:",lifp->ac_enaddr.ether_addr_octet[j]);
	printf("\t %d\n",lifp->speed);
	lifp = lifp->next;
    }
}

/* Do the initial read of the interface table and makeup local table. */
int
  if_init()
{
    invalidate_ifcache();

    if (mibopen() == 0)
      return -1;

    lif = LIF_NULL;
    ifnumber = 0;

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
mib2_ipAddrEntry_t      *ap;
mib_item_t *item;
int first = 1;
 
int devfd, bw, icnt;
 
struct strbuf ctlbuf,databuf;
char      buf[8192];
char      devname[50];
int       counter;
 
dl_info_req_t dlpi_req;
dl_info_ack_t dlpi_ack;
dl_attach_req_t attach_req;
struct  strbuf  ctl;
int flags;
 


if ((cache_now - if_cache_time) <= CACHE_LIFETIME)
  return;
if_cache_time = cache_now;
if (lif_head.next != LIF_NULL)	/* Free up old data */
   {
   struct lif *lifp;
   struct lif *nxtlifp;
   for (lifp = lif_head.next; lifp;)
      {
      nxtlifp = lifp->next;
      (void) free(lifp);
      lifp = nxtlifp;
      }
   }

ifnumber = 0;
if (item_if) 
  mibfree(item_if);
if ((item_if = (mib_item_t *)mibget(MIB2_IP)) == nilp(mib_item_t))
   return;
for (item = item_if ; item; item=item->next_item) {
   if ((item->group == MIB2_IP) && (item->mib_id == MIB2_IP_20))  {
      ap = (mib2_ipAddrEntry_t *)item->valp;
      break;
   }
}
if (!item) {
  mibfree(item_if);
  return;
}
for (; (char *)ap < item->valp + item->length; ap++) 
   {
   struct lif *lp,*tmp;
   int i;

   ifnumber++;
   if (first) 
   {
	lp = &lif_head;
	first = 0;
   }
   else 
   {
	if ((tmp = (struct lif *)malloc(sizeof(struct lif))) == 0)
		return;
	(void)memset((char *)tmp, 0, sizeof(struct lif));
	lp->next = tmp;
	lp = tmp;
   }
   lp->koff = (off_t)ifnumber; 
   lp->ipAddr = (mib2_ipAddrEntry_t *)ap;
   memcpy(&lp->name,&lp->ipAddr->ipAdEntIfIndex.o_bytes,
				lp->ipAddr->ipAdEntIfIndex.o_length);
   lp->status = ((lp->ipAddr->ipAdEntInfo.ae_flags) & IFF_UP) ? 1 : 2;
   if ((lp->ipAddr->ipAdEntInfo.ae_flags) & IFF_LOOPBACK)
      {
      lp->type = 24;		/* software loopback */
      lp->speed = 10000000;
      }
   else {
      memset(devname, 0, sizeof(devname));       
      memcpy(devname, "/dev/", 5);
      for(counter=0; counter < lp->ipAddr->ipAdEntIfIndex.o_length; counter++){
            if (!isdigit(lp->name[counter]))
                 devname[counter+5] = lp->name[counter];
            else
                 break;
      }
      devfd = open(devname, O_RDWR);
      if (devfd == -1) {
          lp->type = 1;           /* other */
          lp->speed = 9600;
      }else {
          dlpi_req.dl_primitive = DL_INFO_REQ;
 
          ctlbuf.buf = (char *)&dlpi_req;
          ctlbuf.len = sizeof(dl_info_req_t);

          if (putmsg(devfd, &ctlbuf , (struct strbuf *)NULL, 0) == -1){
                  lp->type = 1;           /* other */
                  lp->speed = 9600;
          }
          else {
              ctlbuf.buf = (char *)&dlpi_ack;
              ctlbuf.maxlen = DL_INFO_ACK_SIZE;
    
              databuf.buf = buf;
              databuf.maxlen = sizeof(buf);
              databuf.len = sizeof(buf);
              if (getmsg(devfd, &ctlbuf, &databuf , &flags) == -1) {
                  lp->type = 1;           /* other */
                  lp->speed = 9600;
              }else {
                  switch(dlpi_ack.dl_mac_type) {
                      case DL_CSMACD:
      				lp->type = 7;		/* 802.3 */
      				lp->speed = 10000000;
      				lp->flags |= LIF_HAS_PHYSADDR;
      		/* The following gets the Ethernet address.		*/
     get_physical_addr(lp->ac_enaddr.ether_addr_octet, lp->ipAddr->ipAdEntAddr);
				break;
                      case DL_ETHER:
      				lp->type = 6;		/* ethernet */
      				lp->speed = 10000000;
      				lp->flags |= LIF_HAS_PHYSADDR;
      		/* The following gets the Ethernet address.		*/
     get_physical_addr(lp->ac_enaddr.ether_addr_octet, lp->ipAddr->ipAdEntAddr);
				break;
                      case DL_TPB:
                            lp->type = 8;
                            lp->speed = 100000000;
                            break;
                      case DL_TPR:
                            lp->type = 9;
                            lp->speed = 100000000;
                            break;
                      case DL_FDDI:
                            lp->type = 15;
                            lp->speed = 100000000;
                          /* Exact speed for SunATM */
                            attach_req.dl_primitive = DL_ATTACH_REQ;
                            attach_req.dl_ppa = 0;
 
                            ctl.maxlen = 0;
                            ctl.len = sizeof (attach_req);
                            ctl.buf = (char *) &attach_req;
 
                            flags = 0;
 
                            if (putmsg(devfd, &ctl, NULL, flags) < 0) {
                                 break;
                            }
 
 
                            tmpstrioctl.ic_cmd = (('A' << 8) | 29);/*A_GET_LINK_BW;*/
                            tmpstrioctl.ic_timout = -1;
                            tmpstrioctl.ic_len = sizeof (bw);
                            tmpstrioctl.ic_dp = (char *) &bw;
       
                            if (ioctl(devfd, I_STR, &tmpstrioctl) < 0) {
                                 break; 
                            }else{
                               lp->speed = bw * 1000000L;
                            }
 
                            break;
                      case DL_METRO:
                      case DL_HDLC:
                      case DL_CHAR:
                      case DL_CTCA:
                      case DL_OTHER:
                            lp->type = 1;
                            lp->speed = 9600;
                            break;

                  } 
              }
             
          }
 
       }
 
     close(devfd);   
   }
   for( icnt=0; icnt< new_device_pointer; icnt++) {
         if(!strcmp(&devname[5], new_devices[i].name))
              break;
   }
 
   if( icnt < new_device_pointer) {
      lp->type = new_devices[i].type;
      lp->speed = new_devices[i].speed;
   } 

#if 0
   else if ((strncmp(lp->name, "ie", 2) == 0) || /* Intel 82586 */
	    (strncmp(lp->name, "le", 2) == 0) || /* AMD LANCE */
	    (strncmp(lp->name, "ec", 2) == 0))   /* 3-COM */
      {
      struct arpcom arpcom;
      lp->type = 6;		/* ethernet */
      lp->speed = 10000000;
      lp->flags |= LIF_HAS_PHYSADDR;
      /* The following gets the Ethernet address.		*/
      get_physical_addr(lp->ac_enaddr.ether_addr_octet, lp->ipAddr->ipAdEntAddr);
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
      lp->speed = 16000000;	/* speed either 16000000 or 4000000 */
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
#endif
   update_counters(lp);
   }
   lif = &lif_head;
}

void
update_counters(lp)
struct lif *lp;
{
   kstat_ctl_t *kc;
   kstat_t *ksp;
   kstat_named_t *knp;

   if ((kc = kstat_open()) == NULL) 
	return;
   if ((ksp = kstat_lookup(kc, NULL, -1, lp->name )) != NULL)
	kstat_read(kc, ksp, NULL);
   else {
	kstat_close(kc);
	return;
   }
   if (knp = kstat_data_lookup(ksp, "ipackets"))
	   lp->if_ipackets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "ierrors"))
	   lp->if_ierrors = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "opackets"))
	   lp->if_opackets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "oerrors"))
	   lp->if_oerrors = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "collisions"))
	   lp->if_collisions = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "queue"))
	   lp->if_qlen = knp->value.ul;

   if (knp = kstat_data_lookup(ksp, "rbytes"))
	   lp->if_ioctets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "obytes"))
	   lp->if_ooctets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "multircv"))
	   lp->if_inupackets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "brdcstrcv"))
	   lp->if_inupackets += knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "multixmt"))
	   lp->if_onupackets = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "brdcstxmt"))
	   lp->if_onupackets += knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "norcvbuf"))
	   lp->if_idiscards = knp->value.ul;
   if (knp = kstat_data_lookup(ksp, "noxmtbuf"))
	   lp->if_odiscards = knp->value.ul;

 
   if (lp->if_ioctets == 0) {
      if ((ksp = kstat_lookup(kc, "lmgt", -1, lp->name )) != NULL) {
         kstat_read(kc, ksp, NULL);
      if (knp = kstat_data_lookup(ksp, "rbytes"))
            lp->if_ioctets = knp->value.ul;
      if (knp = kstat_data_lookup(ksp, "obytes"))
            lp->if_ooctets = knp->value.ul;
      }
   }

   kstat_close(kc);

}


void
get_physical_addr(ether,addr)
char *ether;
u_long addr;
{
mib_item_t *item, *item_addr;
mib2_ipNetToMediaEntry_t *ipNet;
int i;

if ((item_addr = (mib_item_t *)mibget(MIB2_IP)) == nilp(mib_item_t))
   return;
for (item = item_addr ; item; item=item->next_item) 
   {
      if ((item->group == MIB2_IP) && (item->mib_id == MIB2_IP_22))  
         {
            ipNet = (mib2_ipNetToMediaEntry_t *)item->valp;
            break;
         }
   }
if (!item) {
  mibfree(item_addr);
  return;
}
for (; (char *)ipNet < item->valp + item->length; ipNet++)
      if (ipNet->ipNetToMediaNetAddress == addr) 
         {
		memcpy(ether,&ipNet->ipNetToMediaPhysAddress.o_bytes,
				ipNet->ipNetToMediaPhysAddress.o_length);
		
		mibfree(item_addr);
		return;
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
   if (lifp->flags & LIF_NO_ADDRESS)
      continue;
   if (iap->s_addr == lifp->ipAddr->ipAdEntAddr)
      return lifp;
   }
return LIF_NULL;
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
  get_interface_number(unsigned long addr)
#else   /* NO_PP */
int
  get_interface_number(addr)
	unsigned long addr;
#endif  /* NO_PP */
{
    struct lif *lifp;
    int i;

    for (lifp = lif, i = 1; lifp && i <= ifnumber; i++, lifp = lifp->next) {
	if (addr == lifp->ipAddr->ipAdEntAddr)
	  return i;
    }
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

    return lifp->ipAddr->ipAdEntInfo.ae_mtu;
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

(void) gettimeofday(&now_is, NULL);
tticks_now = (unsigned long)(((now_is.tv_sec - boot_at.tv_sec) * 100) +
			     (now_is.tv_usec / 10000));

switch(value)
   {
   case 1:	/* Turn interface on */
	   lifp->ipAddr->ipAdEntInfo.ae_flags |= IFF_UP;
	   invalidate_ifcache();
	   if (if_req.ifr_flags & IFF_UP) return; /* Already up */
	   if_req.ifr_flags |= IFF_UP;
	   lifp->change_time = tticks_now;
	   (void)ioctl(snmp_socket, SIOCSIFFLAGS, &if_req);
	   send_traps(snmp_socket, LINK_UP, compl[0]);
	   break;
   case 2:	/* Turn interface off */
   case 3:	/* Turn interface to "test mode" */
	   lifp->ipAddr->ipAdEntInfo.ae_flags &= ~IFF_UP;
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

    return lifp->ipAddr->ipAdEntInfo.ae_flags & IFF_UP ? 1 : 2;
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
  get_ifInOctets(OIDC_T lastmatch,
		       int compc,
		       OIDC_T *compl,
		       char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifInOctets(lastmatch, compc, compl, cookie)
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

    return (lifp->if_ioctets);
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

    return lifp->if_ipackets;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifInNUcastPkts(OIDC_T lastmatch,
                       int compc,
                       OIDC_T *compl,
                       char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifInNUcastPkts(lastmatch, compc, compl, cookie)
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
        
    return lifp->if_inupackets;
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

    return lifp->if_ierrors;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifInDiscards(OIDC_T lastmatch,
                     int compc,
                     OIDC_T *compl,
                     char *cookie)
#else   /* NO_PP */  
/*ARGSUSED*/
UINT_32_T
  get_ifInDiscards(lastmatch, compc, compl, cookie)
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
 
    return lifp->if_idiscards;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutOctets(OIDC_T lastmatch,
			int compc,
			OIDC_T *compl,
			char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifOutOctets(lastmatch, compc, compl, cookie)
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

    return (lifp->if_ooctets);
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

    return lifp->if_opackets;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutNUcastPkts(OIDC_T lastmatch,
                        int compc,
                        OIDC_T *compl,
                        char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
  get_ifOutNUcastPkts(lastmatch, compc, compl, cookie)
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
 
    return lifp->if_onupackets;
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

    return lifp->if_oerrors + lifp->if_collisions;
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
  get_ifOutDiscards(OIDC_T lastmatch,
                     int compc,
                     OIDC_T *compl,
                     char *cookie)
#else   /* NO_PP */  
/*ARGSUSED*/
UINT_32_T
  get_ifOutDiscards(lastmatch, compc, compl, cookie)
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
 
    return (lifp->if_odiscards);
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

    return lifp->if_qlen;
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
