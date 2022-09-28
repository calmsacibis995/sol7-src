/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)tcp.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)tcp.c	2.15 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/TCP.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:26
 * Initial revision.
 * 
*/

#include <stdio.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include  <sys/types.h>
#include  <sys/protosw.h>
#include  <sys/socket.h>
#include  <net/route.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#include  <netinet/in_systm.h>
#include  <netinet/in_pcb.h>
#include  <netinet/ip.h>
#include  <netinet/ip_var.h>
#include  <netinet/tcp.h>
#include  <netinet/tcp_timer.h>
#include  <netinet/tcp_var.h>
#include  <netinet/tcp_fsm.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

#define CACHE_LIFETIME		45
#define CSECS_PER_PR_SLOWHZ	(100 / PR_SLOWHZ)

#define INSTANCE_LEN 10
#define LOCAL_ADDR 0
#define LOCAL_PORT 4
#define REMOTE_ADDR 5
#define REMOTE_PORT 9

static struct kernel_symbol tcpstat = { "_tcpstat", 0, 0 };
static struct tcpstat tcps;

static struct kernel_symbol tcp_cons = { "_tcb", 0, 0 };
static struct inpcb *tcb = 0;
static time_t tcp_cache_time = 0;
#if !defined(NO_PP)
static int    read_tcp_ct(void);
#else   /* NO_PP */
static int    read_tcp_ct();
#endif  /* NO_PP */

static
int read_tcp_ct()
{
/* Get the connection table ... */
if ((tcb == (struct inpcb *)0) ||
    ((cache_now - tcp_cache_time) > CACHE_LIFETIME))
  {
  tcp_cache_time = cache_now;
  if (find_loc(&tcp_cons) == 0)
    return -1;
  (void) read_ct(&tcb, tcp_cons.offset);
  }
return 0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T get_tcp_ints(OIDC_T lastmatch, int compc,
			  OIDC_T *compl, char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T get_tcp_ints(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
if (find_loc(&tcpstat) == 0)
  return (INT_32_T)0;

read_bytes((off_t)tcpstat.offset, (char *)&tcps, sizeof(struct tcpstat));

switch (lastmatch)
  {
  case 1:	/* tcpRtoAlgorithm	*/
      /* rsre(3).  How do you tell when host upgrades */
      return (INT_32_T)4;
  case 2:	/* tcpRtoMin		*/
      return (INT_32_T)(TCPTV_SRTTDFLT * CSECS_PER_PR_SLOWHZ);
  case 3:	/* tcpRtoMax		*/
      return (INT_32_T)(TCPTV_MSL * CSECS_PER_PR_SLOWHZ);
  case 4:	/* tcpMaxConn		*/
      return (INT_32_T)-1;
  default:
      return (INT_32_T)-1;
  }
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T get_tcp_uints(OIDC_T lastmatch, int compc,
			  OIDC_T *compl, char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T get_tcp_uints(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
if (find_loc(&tcpstat) == 0)
  return (INT_32_T)0;

read_bytes((off_t)tcpstat.offset, (char *)&tcps, sizeof(struct tcpstat));

switch (lastmatch)
  {
  case 4:	/* tcpMaxConn		*/
      return (INT_32_T)-1;
  case 5:	/* tcpActiveOpens	*/
      return (INT_32_T)tcps.tcps_connects;
  case 6:	/* tcpPassiveOpens	*/
      return (INT_32_T)tcps.tcps_accepts;
  case 7:	/* tcpAttemptFails	*/
      return (INT_32_T)tcps.tcps_conndrops;
  case 8:	/* tcpEstabResets	*/
      return (INT_32_T)tcps.tcps_drops;
  case 9:	/* tcpCurrEstab		*/
      return (INT_32_T)-1;	/* ought never to get here */
  case 10:	/* tcpInSegs		*/
      return (INT_32_T)tcps.tcps_rcvtotal;
  case 11:	/* tcpOutSegs		*/
      return (INT_32_T)tcps.tcps_sndtotal;
  case 12:	/* tcpRetransSegs	*/
      return (INT_32_T)tcps.tcps_sndrexmitpack;
  case 14:	/* tcpInErrs		*/
      return (INT_32_T)(tcps.tcps_rcvbadsum
			+ tcps.tcps_rcvbadoff
			+ tcps.tcps_rcvshort);
  case 15:	/* tcpOutRsts		*/
      return (INT_32_T)0;
  default:
      return (INT_32_T)0;
  }
/*NOTREACHED*/
}

#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
get_tcpCurrEstab(OIDC_T lastmatch, int compc,
		    OIDC_T *compl, char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
UINT_32_T
get_tcpCurrEstab(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
struct tcpcb	tcpcb;
struct inpcb *	pcb;
unsigned int	estab_cnt = 0;

if (read_tcp_ct() == -1) return 0;

for (pcb = tcb; pcb; pcb = pcb->inp_next)
  {
  read_bytes((off_t)(pcb->inp_ppcb), (char *)&tcpcb, sizeof(tcpcb));
  if ((tcpcb.t_state == (short)TCPS_ESTABLISHED) ||
      (tcpcb.t_state == (short)TCPS_CLOSE_WAIT)) estab_cnt++;
  }
return estab_cnt;
}

/* Define a table to map from the TCP states as defined in	*/
/* /usr/include/netinet/tcp_fsm.h to those in the TCP/IP MIB.	*/
typedef struct tcpstatemap_s
	{
	short tcp_state;
	short mib_state;
	} tcpstatemap_t;

static tcpstatemap_t tstate_map[TCP_NSTATES] = {
    			{TCPS_CLOSED, 1},
			{TCPS_LISTEN, 2},
			{TCPS_SYN_SENT, 3},
			{TCPS_SYN_RECEIVED, 4},
			{TCPS_ESTABLISHED, 5},
			{TCPS_CLOSE_WAIT, 8},
			{TCPS_FIN_WAIT_1, 6},
			{TCPS_CLOSING, 8},
			{TCPS_LAST_ACK, 9},
			{TCPS_FIN_WAIT_2, 7},
			{TCPS_TIME_WAIT, 11}};

/*ARGSUSED*/
INT_32_T
  get_tcpConnState(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
    struct in_addr	localip;
    struct in_addr	remoteip;
    unsigned int	localport;
    unsigned int	remoteport;
    struct tcpcb	tcpcb;
    struct inpcb *	pcb;
    
    /* The unused components must represent valid IP addresses and TCP port #'s */
    localip = frungulate(4, compl+LOCAL_ADDR);
    localport = compl[LOCAL_PORT];
    remoteip = frungulate(4, compl+REMOTE_ADDR);
    remoteport = compl[REMOTE_PORT];
    
    /* The connection table is assumed to have been read-in by either	*/
    /* tcptable_test() or tcptable_next().			*/
    for (pcb = tcb; pcb; pcb = pcb->inp_next) {
	if ((localport == pcb->inp_lport) &&
	    (localip.s_addr == pcb->inp_laddr.s_addr) &&
	    (remoteport == pcb->inp_fport) &&
	    (remoteip.s_addr == pcb->inp_faddr.s_addr)) {
	    tcpstatemap_t * tmapp;
	    int i;
	    read_bytes((off_t)(pcb->inp_ppcb), (char *)&tcpcb, sizeof(tcpcb));
	    /* Map the internal state numbers to the mib state numbers */
	    for (i = 0, tmapp = tstate_map; i < TCP_NSTATES; i++, tmapp++)
		{
		if (tmapp->tcp_state == tcpcb.t_state) return tmapp->mib_state;
		}
	    return 0; /* Can't find a state!!! */
	}
    }
    return 0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_tcpConnState(OIDC_T		lastmatch,
		 int		compc,
		 OIDC_T		*compl,
		 char		*cookie,
		 INT_32_T	value,
		 SNMP_PKT_T *	pktp,
		 int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_tcpConnState(lastmatch, compc, compl, cookie, value, pktp, index)
		 OIDC_T		lastmatch;
		 int		compc;
		 OIDC_T		*compl;
		 char		*cookie;
		 INT_32_T	value;
		 SNMP_PKT_T *	pktp;
		 int		index;
#endif	/* NO_PP */
{
/* *(int *)cookie = (int)value; */
}

/*ARGSUSED*/
INT_32_T
  get_tcptab_localport(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    return (INT_32_T)(compl[LOCAL_PORT]);
}

/*ARGSUSED*/
INT_32_T
  get_tcptab_remoteport(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {

    return (INT_32_T)(compl[REMOTE_PORT]);
}

/*ARGSUSED*/
unsigned char *
  get_tcptab_localaddr(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
    static struct in_addr	localip;

    localip = frungulate(4, compl+LOCAL_ADDR);
    return (unsigned char *)(&localip);
}

/*ARGSUSED*/
unsigned char *
  get_tcptab_remoteaddr(lastmatch, compc, compl, cookie)
OIDC_T lastmatch;
int compc;
OIDC_T *compl;
char *cookie; {
    static struct in_addr	remoteip;

    remoteip = frungulate(4, compl+REMOTE_ADDR);
    return (unsigned char *)(&remoteip);
}

/****************************************************************************
NAME:  tcptable_test

PURPOSE:  Test whether a given object exists in the TCP table

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
tcptable_test(form, last_match, compc, compl, cookie)
	int	form;	/* See TEST_SET & TEST_GET in mib.h	*/
	OIDC_T	last_match; /* Last component matched */
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
{
struct in_addr	localip;
struct in_addr	remoteip;
unsigned int	localport;
unsigned int	remoteport;
struct inpcb *	pcb;

/* There must be exactly 10 unused components */
if (compc != INSTANCE_LEN) return -1;

/* The unused components must represent valid IP addresses and TCP port #'s */
localip = frungulate(4, compl+LOCAL_ADDR);
localport = (unsigned int)compl[LOCAL_PORT];
remoteip = frungulate(4, compl+REMOTE_ADDR);
remoteport = (unsigned int)compl[REMOTE_PORT];

if (last_match > 5)
  return -1;

if (read_tcp_ct() == -1) return -1;

for (pcb = tcb; pcb; pcb = pcb->inp_next) {
    if ((localport == pcb->inp_lport) &&
	(localip.s_addr == pcb->inp_laddr.s_addr) &&
	(remoteport == pcb->inp_fport) &&
	(remoteip.s_addr == pcb->inp_faddr.s_addr))
      return 0;
}
return -1;
}

/****************************************************************************
NAME:  tcptable_next

PURPOSE:  Locate the "next" object in the TCP table

PARAMETERS:
	OIDC_T		Last component of the object id leading to 
			the leaf node in the MIB.  This identifies
			the particular "attribute" we want in the table.
			The following values give an example:
				 1 - tcpConnState
				 2 - tcpConnLocalAddress
				 3 - tcpLocalPort
				 4 - tcpConnRemAddress
				 5 - tcpRemPort

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
tcptable_next(last_match, tcount, tlist, rlist, cookie)
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

if (read_tcp_ct() == -1) return 0;

/* Now find the lowest value larger than localip and localport. */
best = (struct inpcb *)0;
for (pcb = tcb; pcb; pcb = pcb->inp_next) {
    ip_to_rlist(pcb->inp_laddr, (OIDC_T *)(pra+LOCAL_ADDR));
    pra[LOCAL_PORT] = pcb->inp_lport;
    ip_to_rlist(pcb->inp_faddr, (OIDC_T *)(pra+REMOTE_ADDR));
    pra[REMOTE_PORT] = pcb->inp_fport;

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
    ip_to_rlist(best->inp_faddr, rlist+REMOTE_ADDR);
    rlist[REMOTE_PORT] = best->inp_fport;
    return INSTANCE_LEN;
}

/* Pass on it */
return 0;
}

int
tcp_init()
{
if (find_loc(&tcp_cons) == 0) return -1;
if (find_loc(&tcpstat) == 0) return -1;
return 0;
}
