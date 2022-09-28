/*
 * sendbp.c: "Construct and send BOOTP packet".
 *
 * SYNOPSIS
 *    int sendBp(IFINSTANCE *anIf, int broadcast)
 *
 * DESCRIPTION
 *    Construct and send the DHCP packet implied by the contents
 *    of the Host structure for the interface.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)sendbp.c 1.5 96/12/26 SMI"

#include <string.h>
#include "client.h"
#include "dccommon.h"
#include "dhcp.h"
#include "hostdefs.h"
#include "error.h"
#include "bootp.h"
#include "msgindex.h"
#include "unixgen.h"
#include "ca_time.h"
#include <sys/socket.h>
#ifdef	DEBUG
#include "utils.h"
#endif	/* DEBUG */

#if DEBUG
static const char *dbg[] = {
/* 00 */ "xid=%0#10lx secs=%hd flags=%0#4x\n",
/* 01 */ "bootp packet:\n",
/* 02 */ "\nsending to <sockaddr> <%s>\n",
0
};

static void
debugPrintA(int type, const struct bootp *pbp, const struct Host *ph)
{
	if (debug >= 2)
		logb(dbg[0], (u_long)ntohl(pbp->xid), ntohs(pbp->secs),
		    ntohs(pbp->flags));
	if (debug >= 3)
		dump_host(stdbug, ph, 1, ':');
	if (debug >= 5)
		YCDISP(stdbug, pbp, dbg[1], sizeof (struct bootp));
}
#endif


int
sendBp(IFINSTANCE *anIf, int broadcast)
{
	struct bootp sendbootp;
	int rc;

	HostToBootp(anIf->ifsendh, &sendbootp, OVERLOAD_BOTH,
	    sizeof (struct bootp));
	sendbootp.op = BOOTREQUEST;
	sendbootp.secs = GetCurrentSecond() - anIf->ifBegin;
	sendbootp.secs = htons(sendbootp.secs); /* in case htons is a macro */
	sendbootp.xid = anIf->ifXid;
	sendbootp.xid = htonl(sendbootp.xid); /* watch out for htonl macros */

#if DEBUG
	if (debug >= 2)
		logb(dbg[2], addrstr(&anIf->ifsaib,
		    sizeof (struct sockaddr_in), 0, '.'));
#endif

	/*
	 * Broadcast if we are pretending to be at a different MAC address
	 * because the unicast reply will not reach us
	 */
	if (broadcast && client.UserDefinedMacAddr != 0)
		sendbootp.flags = htons(BOOTP_BROADCAST_BIT);

	if (broadcast)
		rc = liBroadcast(&anIf->lli, &sendbootp, sizeof (struct bootp),
		    0);
	else
		rc = sendto(anIf->fd, (char *)&sendbootp,
		    sizeof (struct bootp), 0, (struct sockaddr *)&anIf->ifsaib,
		    sizeof (struct sockaddr));

	if (rc != sizeof (struct bootp)) {
		loge(DHCPCMSG35, DHCPmessageTypeString(
		    anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1), SYSMSG);
		return (ERR_SEND);
	}

#if DEBUG
	debugPrintA(anIf->ifState, &sendbootp, anIf->ifsendh);
#endif
	return (0);
}
