/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)tcp-svr4.c	1.12 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)tcp-svr4.c	1.12 96/10/02 Sun Microsystems";
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
#include  <netdb.h>
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

static mib2_tcp_t  *tcp = NULL;
static mib2_tcpConnEntry_t  *tcpct_table = NULL;
static int tcpconns = 0;
mib_item_t *item_tcp = 0;
static struct inpcb *tcb = 0;
static time_t tcp_cache_time = 0;
#if !defined(NO_PP)
static int    read_tcp_ct(void);
#else   /* NO_PP */
static int    read_tcp_ct();
#endif  /* NO_PP */

void
pr_tcp()
{
   int i=0;
   struct hostent *hp;
   mib2_tcpConnEntry_t *ct;
   extern  mib2_ip_t *ips;
   extern int read_ip_cache();

   printf("---- tcp ----\n");
   printf("tcpRtoMin %d\n",tcp->tcpRtoMin);
   printf("tcpRtoMax %d\n",tcp->tcpRtoMax);
   printf("tcpMaxConn %d\n",tcp->tcpMaxConn);
   printf("tcpActiveOpens %d\n",tcp->tcpActiveOpens);
   printf("tcpPassiveOpens %d\n",tcp->tcpPassiveOpens);
   printf("tcpAttemptFails %d\n",tcp->tcpAttemptFails);
   printf("tcpEstabResets %d\n",tcp->tcpEstabResets);
   printf("tcpCurrEstab %d\n",tcp->tcpCurrEstab);
   printf("tcpInSegs %d\n",tcp->tcpInSegs);
   printf("tcpOutSegs %d\n",tcp->tcpOutSegs);
   printf("tcpRetransSegs %d\n",tcp->tcpRetransSegs);
   printf("tcpConnTableSize %d\n",tcp->tcpConnTableSize);
   printf("tcpOutRsts %d\n",tcp->tcpOutRsts);
   read_ip_cache();
   printf("tcpInErrs %d\n",ips->tcpInErrs);
   printf("\n---- tcp connection table %d\n",tcpconns);
   printf("#  %-15s %-15s localport remoteport\n",
		"source","dest");
   for (i=1, ct = tcpct_table;i <= tcpconns && ct;i++,ct++) {
	if (ct->tcpConnState < MIB2_TCP_established) 
		continue;
	hp = gethostbyaddr((char *)&ct->tcpConnLocalAddress,4,AF_INET);
	printf("%2d  %-15s ",i,hp->h_name);
	hp = gethostbyaddr((char *)&ct->tcpConnRemAddress,4,AF_INET);
        printf("%-15s   %-5d     %-5d\n",hp->h_name,
			ct->tcpConnLocalPort,ct->tcpConnRemPort);
     }
   
}

static
int read_tcp_ct()
{
mib2_tcpConnEntry_t *ct;
mib_item_t *item;

if ((cache_now - tcp_cache_time) <= CACHE_LIFETIME)
   return 0;
mibfree(item_tcp);
tcp_cache_time = cache_now;
tcp = NULL;
tcpct_table = NULL;

if ((item_tcp = (mib_item_t *)mibget(MIB2_TCP)) == NULL)
   return -1;
for ( item = item_tcp; item; item=item->next_item) {
   if (item->group == MIB2_TCP)  {
      tcp = (mib2_tcp_t *)item->valp;
      break;
   }
}
if (!tcp)
   return -1;
for ( ; item; item=item->next_item) {
   if (item->group == MIB2_TCP && item->mib_id == MIB2_TCP_13)  {
      tcpct_table = (mib2_tcpConnEntry_t *)item->valp;
      break;
   }
}
if (!tcpct_table)
  return -1;
ct = tcpct_table;
tcpconns = 0;
while ((char *)ct < item->valp + item->length) {
	tcpconns++;
	ct++;
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

if (read_tcp_ct() == -1) return -1;

switch (lastmatch)
  {
  case 1:	/* tcpRtoAlgorithm	*/
      /* rsre(3).  How do you tell when host upgrades */
      return (INT_32_T)tcp->tcpRtoAlgorithm;
  case 2:	/* tcpRtoMin		*/
      return (INT_32_T)tcp->tcpRtoMin;
  case 3:	/* tcpRtoMax		*/
      return (INT_32_T)tcp->tcpRtoMax;
  case 4:	/* tcpMaxConn		*/
      return (INT_32_T)tcp->tcpMaxConn;
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
extern mib2_ip_t *ips;
extern int read_ip_cache();

if (read_tcp_ct() == -1) return -1;

switch (lastmatch)
  {
  case 4:	/* tcpMaxConn		*/
      return (INT_32_T)tcp->tcpMaxConn;
  case 5:	/* tcpActiveOpens	*/
      return (INT_32_T)tcp->tcpActiveOpens;
  case 6:	/* tcpPassiveOpens	*/
      return (INT_32_T)tcp->tcpPassiveOpens;
  case 7:	/* tcpAttemptFails	*/
      return (INT_32_T)tcp->tcpAttemptFails;
  case 8:	/* tcpEstabResets	*/
      return (INT_32_T)tcp->tcpEstabResets;
  case 9:	/* tcpCurrEstab		*/
      return (INT_32_T)tcp->tcpCurrEstab;	/* ought never to get here */
  case 10:	/* tcpInSegs		*/
      return (INT_32_T)tcp->tcpInSegs;
  case 11:	/* tcpOutSegs		*/
      return (INT_32_T)tcp->tcpOutSegs;
  case 12:	/* tcpRetransSegs	*/
      return (INT_32_T)tcp->tcpRetransSegs;
  case 14:	/* tcpInErrs		*/
      read_ip_cache();
      return (INT_32_T)ips->tcpInErrs;
  case 15:	/* tcpOutRsts		*/
      return (INT_32_T)tcp->tcpOutRsts;
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
return (UINT_32_T)tcp->tcpCurrEstab;
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
    mib2_tcpConnEntry_t *ct;
    int i;
    
    /* The unused components must represent valid IP addresses and TCP port #'s */
    localip = frungulate(4, compl+LOCAL_ADDR);
    localport = compl[LOCAL_PORT];
    remoteip = frungulate(4, compl+REMOTE_ADDR);
    remoteport = compl[REMOTE_PORT];
    
    /* The connection table is assumed to have been read-in by either	*/
    /* tcptable_test() or tcptable_next().			*/
    for (i=0, ct = tcpct_table;i < tcpconns && ct;i++,ct++) {
	if ((localport == ct->tcpConnLocalPort) &&
		    (localip.s_addr == ct->tcpConnLocalAddress) &&
		    (remoteport == ct->tcpConnRemPort) &&
		    (remoteip.s_addr == ct->tcpConnRemAddress)) {
	    return ct->tcpConnState;
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
mib2_tcpConnEntry_t *ct;
int i;

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
for (i=0, ct = tcpct_table;i < tcpconns && ct;i++,ct++) {
    if ((localport == ct->tcpConnLocalPort) &&
	(localip.s_addr == ct->tcpConnLocalAddress) &&
	(remoteport == ct->tcpConnRemPort) &&
	(remoteip.s_addr == ct->tcpConnRemAddress))
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
mib2_tcpConnEntry_t *ct;
mib2_tcpConnEntry_t *best;
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

if (read_tcp_ct() == -1) return 0;

/* Now find the lowest value larger than localip and localport. */
best = (mib2_tcpConnEntry_t *)0;
for (j=0, ct = tcpct_table;j < tcpconns && ct;j++,ct++) {
    in.s_addr = ct->tcpConnLocalAddress;
    ip_to_rlist(in, (OIDC_T *)(pra+LOCAL_ADDR));
    pra[LOCAL_PORT] = ct->tcpConnLocalPort;
    in.s_addr = ct->tcpConnRemAddress;
    ip_to_rlist(in, (OIDC_T *)(pra+REMOTE_ADDR));
    pra[REMOTE_PORT] = ct->tcpConnRemPort;

    if (objidcmp(pra, lra, INSTANCE_LEN) <= 0)
      continue;
    if (!best || (objidcmp(pra, bra, INSTANCE_LEN) < 0)) {
	best = ct;
	for (i = 0; i < INSTANCE_LEN; i++)
	  bra[i] = pra[i];
    }
}

if (best) {
    in.s_addr = best->tcpConnLocalAddress;
    ip_to_rlist(in, rlist+LOCAL_ADDR);
    rlist[LOCAL_PORT] = best->tcpConnLocalPort;
    in.s_addr = best->tcpConnRemAddress;
    ip_to_rlist(in, rlist+REMOTE_ADDR);
    rlist[REMOTE_PORT] = best->tcpConnRemPort;
    return INSTANCE_LEN;
}

/* Pass on it */
return 0;
}

int
tcp_init()
{
if (mibopen() == 0)
   return -1;
read_tcp_ct();
return 0;
}
