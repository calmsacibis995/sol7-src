/* Copyright 1988 - 01/28/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)general.h	2.20 97/01/28 Sun Microsystems"
#endif

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
 * $Log:   E:/SNMPV2/AGENT/SUN/GENERAL.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:28
 * Initial revision.
 * 
*/

#ifdef SVR4
#include <inet/mib2.h>
#else
#include "../h/mib2.h"
#endif

struct kernel_symbol {
    char *name;
    off_t offset;
    short flags;
};

typedef struct mib_item_s {
    struct mib_item_s   * next_item;
    long            group;
    long            mib_id;
    long            length;
    char            * valp;
} mib_item_t;

#define KO_VALID 1
#define KO_FAILED 2

/* Cache structure for interface list.  The interface list is gotten from the
 * kenel at startup.  The locations of the interface structures are remembered
 * here and the list is not read again.  This way, if the interface list
 * changes, the SNMP doesn't go changing interface numbers all around. */
struct lif {
    struct lif *next;	/* next interface in local list */
    /* the *ifp line of code was commented out but needed in if.c */
    struct ifnet *ifp;	/*  local copy of structure */
    mib2_ipAddrEntry_t *ipAddr;
    off_t koff;		/* kernel offset of structure */
    int type;		/* interface type */
    int speed;		/* interface speed */
    int status;		/* admin status */
    char name[IFNAMSIZ]; /* interface name */
    unsigned long       change_time;	/* when entry was last changed,	*/
    					/* in timeticks.		*/
    struct in_addr netmask; /* Subnet mask */
    u_char flags;	/* See below */
    struct ether_addr ac_enaddr;	/* ethernet hardware address */
    int     if_ipackets;            /* packets received on interface */
    int     if_ierrors;             /* input errors on interface */
    int     if_opackets;            /* packets sent on interface */
    int     if_oerrors;             /* output errors on interface */
    int     if_collisions;          /* collisions on csma interfaces */
    int	    if_qlen;
    unsigned long if_ioctets;       /* input octets on interface */
    unsigned long if_ooctets;       /* output octets on interface */
    unsigned long if_inupackets;   /* input non unicast packets on interface */
    unsigned long if_onupackets;   /* output non unicast packets on interface */
    unsigned long if_idiscards;     /* input packet discards */
    unsigned long if_odiscards;     /* output packet discards */
};

/* Values for lif.flags: */
#define	LIF_HAS_PHYSADDR	0x01
#define LIF_NO_ADDRESS		0x02

#define	LIF_NULL	(struct lif *)0

extern struct lif *lif;		/* Local interface list */
extern int ifnumber;		/* number of interfaces */

#if !defined(NO_PP)
extern int read_config(void);
extern off_t find_loc(struct kernel_symbol *);
extern int read_bytes(off_t, char *, int);
extern int write_bytes(off_t, char *, int);
extern int write_int(off_t, int);
extern int read_int(off_t);
extern struct in_addr frungulate(int, unsigned int *);

#if defined(SVR4)
#else	/* (SVR4) */
extern struct lif *getlifbyip(struct in_addr *);
extern struct lif *getlifbycompl(int, OIDC_T *);
extern int getlifindex(struct lif *);
extern void get_if_info(void);
extern int ip_to_rlist(struct in_addr, unsigned int *);
extern void read_ct(struct inpcb **, off_t);
extern void read_if(struct lif *lp);
#endif	/* (SVR4) */

extern int objidcmp(int *, int *, int);
extern int snmpvars_init(void);
extern int libfuncs_init(void);
extern int init(void);
extern int sys_init(void);
extern int ip_init(void);
extern int icmp_init(void);
extern int tcp_init(void);
extern int udp_init(void);
extern int arp_init(void);
extern int if_init(void);
extern int get_interface_number(off_t);
extern int iprte_init(void);
extern void send_traps(int, int, int);
#else	/* NO_PP */
extern int read_config();
extern off_t find_loc();
extern int read_bytes();
extern int write_bytes();
extern int read_int();
extern int ip_to_rlist();
extern struct in_addr frungulate();
extern struct lif *getlifbyip();
extern struct lif *getlifbycompl();
extern int getlifindex();
extern void get_if_info();

extern void read_ct();
extern int objidcmp();
extern int snmpvars_init();
extern int libfuncs_init();
extern int init();
extern int sys_init();
extern int ip_init();
extern int icmp_init();
extern int tcp_init();
extern int udp_init();
extern int arp_init();
extern int read_arptab();
extern int if_init();
extern void read_if();
extern int get_interface_number();
extern int iprte_init();
extern void send_traps();
#endif	/* NO_PP */
