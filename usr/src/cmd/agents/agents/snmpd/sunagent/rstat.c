/*
 * Copyright 1988 - 04/03/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)rstat.c	2.16 97/04/03 Sun Microsystems"
#else
static char sccsid[] = "@(#)rstat.c	2.16 97/04/03 Sun Microsystems";
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
#include <stdio.h>
#include <sys/types.h>
#include  <sys/socket.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>
#include <rpcsvc/rstat.h>

#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>
#include "snmpvars.h"

#include "agent.h"
#include "general.h"




#define	CACHE_LIFETIME 15
static time_t rs_cache_time = 0;
static struct statstime sts;

void
get_rstats()
{
	if ((cache_now - rs_cache_time) <= CACHE_LIFETIME)
		return;
	(void) rstat(snmp_sysName, &sts);
}


/*
 *	ACCESS FUNCTIONS
 */
/* ARGSUSED */
UINT_32_T
get_rsCpuTime1(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.cp_time[RSTAT_CPU_USER]);
}

/* ARGSUSED */
UINT_32_T
get_rsCpuTime2(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.cp_time[RSTAT_CPU_NICE]);
}

/*ARGSUSED*/
UINT_32_T
get_rsCpuTime3(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
get_rstats();
return ((UINT_32_T)sts.cp_time[RSTAT_CPU_SYS]);
}

/* ARGSUSED */
UINT_32_T
get_rsCpuTime4(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.cp_time[RSTAT_CPU_IDLE]);
}

/* ARGSUSED */
UINT_32_T
get_rsDiskXfer1(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.dk_xfer[0]);
}

/* ARGSUSED */
UINT_32_T
get_rsDiskXfer2(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.dk_xfer[1]);
}

/*ARGSUSED*/
UINT_32_T
get_rsDiskXfer3(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.dk_xfer[2]);
}

/* ARGSUSED */
UINT_32_T
get_rsDiskXfer4(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.dk_xfer[3]);
}

/* ARGSUSED */
UINT_32_T
get_rsVPagesIn(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;

{
	get_rstats();
	return ((UINT_32_T)sts.v_pgpgin);
}

/* ARGSUSED */
UINT_32_T
get_rsVPagesOut(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.v_pgpgout);
}

/* ARGSUSED */
UINT_32_T
get_rsVSwapIn(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.v_pswpin);
}

/* ARGSUSED */
UINT_32_T
get_rsVSwapOut(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.v_pswpout);
}

/* ARGSUSED */
UINT_32_T
get_rsVIntr(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.v_intr);
}

/* ARGSUSED */
UINT_32_T
get_rsIfInPackets(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.if_ipackets);
}

/* ARGSUSED */
UINT_32_T
get_rsIfInErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.if_ierrors);
}

/* ARGSUSED */
UINT_32_T
get_rsIfOutPackets(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.if_opackets);
}

/* ARGSUSED */
UINT_32_T
get_rsIfOutErrors(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;

{
	get_rstats();
	return ((UINT_32_T)sts.if_oerrors);
}

/* ARGSUSED */
UINT_32_T
get_rsIfCollisions(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
	get_rstats();
	return ((UINT_32_T)sts.if_collisions);
}
