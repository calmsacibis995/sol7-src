/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)contbl-svr4.c	1.9 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)contbl-svr4.c	1.9 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/CONTBL.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:18
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
#include  <netinet/udp_var.h>

#include "snmpvars.h"
#include "general.h"

/* This is a general utility routine which does an unspeakable thing	*/
/* to a target IP address.						*/
#if !defined(NO_PP)
struct in_addr
frungulate(int tcount, OIDC_T *tlist)
#else   /* NO_PP */
struct in_addr
frungulate(tcount, tlist)
	int tcount;
	OIDC_T *tlist;
#endif  /* NO_PP */
{
struct in_addr		targ_ip;

targ_ip.s_addr = 0L;
if (tcount > 0)
   {
   targ_ip.S_un.S_un_b.s_b1 = (unsigned char)tlist[0];
   if (tcount > 1)
      {
      targ_ip.S_un.S_un_b.s_b2 = (unsigned char)tlist[1];
      if (tcount > 2)
         {
         targ_ip.S_un.S_un_b.s_b3 = (unsigned char)tlist[2];
	 if (tcount > 3)
	    targ_ip.S_un.S_un_b.s_b4 = (unsigned char)tlist[3];
	 }
      }
   }

return targ_ip;
}

#if !defined(NO_PP)
int ip_to_rlist(struct in_addr ip, OIDC_T * rlist)
#else   /* NO_PP */
int ip_to_rlist(ip, rlist)
	struct in_addr ip;
	OIDC_T * rlist;
#endif  /* NO_PP */
{
rlist[0] = (OIDC_T)ip.S_un.S_un_b.s_b1;
rlist[1] = (OIDC_T)ip.S_un.S_un_b.s_b2;
rlist[2] = (OIDC_T)ip.S_un.S_un_b.s_b3;
rlist[3] = (OIDC_T)ip.S_un.S_un_b.s_b4;
return 4;
}

/* Compare two object expandedn object identifiers.			*/
/* Returns -1, 0, of +1 depending whether o1 < o1, o1 == o2, or o1 > o2	*/
#if !defined(NO_PP)
int
objidcmp(int *idp1, int *idp2, int n)
#else   /* NO_PP */
int
objidcmp(idp1, idp2, n)
	int *idp1;
	int *idp2;
	int n;
#endif  /* NO_PP */
{
int	i;

while (n-- > 0)
   {
   if (i = *idp1++ - *idp2++)
     return i;			/* OIDs are different; return signed value */
   }

return 0;	/* OIDs are the same */
}
