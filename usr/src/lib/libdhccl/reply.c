/*
 * reply.c: "Examine DHCP/BOOTP replies".
 *
 * SYNOPSIS
 *    void examineReply(const struct bootp *replybootp, int len,
 *                      const struct sockaddr_in *from)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)reply.c 1.4 96/11/25 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "error.h"
#include "dhcp.h"
#include "hostdefs.h"
#include "unixgen.h"
#include "bootp.h"
#include "msgindex.h"
#include <string.h>

#if DEBUG
static const char *dbg[] = {
/* 0 */ "\nreceived xid=%0#10lx flags=%0#4x from (ip, port#)=(%s, %d)\n",
/* 1 */ "rejected packet with xid=%0#10lx\n",
/* 2 */ "bootp packet\n",
0
};

static void
debugPrintB(int len, const struct bootp *reply, const Host *th,
    const struct sockaddr_in *from)
{
	if (debug >= 2)
		logb(dbg[0], (u_long)ntohl(reply->xid), ntohs(reply->flags),
		    ADDRSTR(&from->sin_addr), from->sin_port);
	if (debug >= 5)
		YCDISP(stdbug, reply, dbg[2], len);
	if (debug >= 3)
		dump_host(stdbug, th, 1, ':');
}
#endif

#if EMPLOY_BOOTP
static void
examineBootp(struct Host *pth, IFINSTANCE *anIf)
{
	switch (anIf->ifState) {
	case DHCP_REBINDING:
	case DHCP_REQUESTING:
	case DHCP_RENEWING:
		free_host_members(pth);
		break;
	case DHCP_REBOOTING:
		free_host_members(pth);
		cancelWakeup(anIf->ifTimerID);
		if (!client.DontConfigure)
			discard_dhc(anIf);
		reinit(anIf);
		break;
	case DHCP_SELECTING:
		if (anIf->ifOffers > 0 || anIf->ifBootp != 0)
			free_host_members(pth);
		else {
			anIf->ifBootp = (Host *)xmalloc(sizeof (Host));
			memcpy(anIf->ifBootp, pth, sizeof (Host));
		}
		break;
	}
}
#endif

/* ARGSUSED */
void
examineReply(const struct bootp *replybootp, int len,
    const struct sockaddr_in *from)
{
	struct Host th;
	IFINSTANCE *anIf;
	uint32_t xid;

	xid = ntohl(replybootp->xid);
	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (anIf->ifXid == xid && !memcmp(anIf->ifMacAddr.chaddr,
		    replybootp->chaddr, replybootp->hlen))
			break;
	}

	if (anIf == 0) {
#if DEBUG
		if (debug >= 3) {
			logb(dbg[1], (u_long)xid);
			BootpToHost(replybootp, &th, len);
			debugPrintB(len, replybootp, &th, from);
			free_host_members(&th);
		}
#endif
		return;
	}

	BootpToHost(replybootp, &th, len);
	anIf->ifReceived++;

#if DEBUG
	debugPrintB(len, replybootp, &th, from);
#endif

#if EMPLOY_BOOTP
	if (!inhost(th.flags, TAG_DHCP_MESSAGE_TYPE)) {
		if (client.acceptBootp) {
			examineBootp(&th, anIf);
			return;
		} else {
			free_host_members(&th);
			return;
		}
	}
#else
	if (!inhost(th.flags, TAG_DHCP_MESSAGE_TYPE)) {
		free_host_members(&th);
		return;
	}
#endif

	switch (anIf->ifState) {
	case DHCP_SELECTING:
		if (th.stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 != DHCPOFFER)
			return;
		if (!inhost(th.flags, TAG_DHCP_SERVER))
			return;

		anIf->ifOffers++;
#if EMPLOY_BOOTP
		if (anIf->ifBootp != 0) {
			free_host(anIf->ifBootp);
			anIf->ifBootp = 0;
		}
#endif
		if (anIf->ifOldConfig != 0 &&
		    anIf->ifOldConfig->stags[TAG_DHCP_SERVER].dhcia.s_addr ==
		    th.stags[TAG_DHCP_SERVER].dhcia.s_addr &&
		    anIf->ifOldConfig->Yiaddr.s_addr == th.Yiaddr.s_addr) {
			if (anIf->ifConfig)
				free_host_members(anIf->ifConfig);
			else
				anIf->ifConfig = (Host *)xmalloc(sizeof (Host));
			if (anIf->ifConfig)
				memcpy(anIf->ifConfig, &th, sizeof (th));
			cancelWakeup(anIf->ifTimerID);
			request(anIf);
			return;
		}

		if (anIf->ifOffers == 1 || (th.stags[TAG_LEASETIME].dhcd4 >
		    client.lease_desired &&
		    anIf->ifConfig->stags[TAG_LEASETIME].dhcd4 <
		    client.lease_desired)) {
			if (anIf->ifConfig)
				free_host_members(anIf->ifConfig);
			else
				anIf->ifConfig = (Host *)xmalloc(sizeof (Host));
			memcpy(anIf->ifConfig, &th, sizeof (th));
		} else
			free_host_members(&th);
		return;
	case DHCP_REBINDING:
	case DHCP_REQUESTING:
	case DHCP_RENEWING:
	case DHCP_REBOOTING:
		if (th.stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 == DHCPNAK) {
			if (inhost(th.flags, TAG_DHCP_ERROR_TEXT))
				logl(DHCPCMSG05, anIf->ifName,
				    th.stags[TAG_DHCP_ERROR_TEXT].dhccp);
			free_host_members(&th);
			if (anIf->ifState == DHCP_REQUESTING) {
				cancelWakeup(anIf->ifTimerID);
				if (++anIf->ifBadOffers == client.retries)
					fail(anIf, ERR_RETRIES);
				else
					reinit(anIf);
			} else if (anIf->ifState == DHCP_REBOOTING) {
				cancelWakeup(anIf->ifTimerID);
				if (!client.DontConfigure)
					discard_dhc(anIf);
				reinit(anIf);
			} else {
				logw(DHCPCMSG60);
				/* XXXX need to take some action here? */
			}
			return;
		} else if (th.stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 == DHCPACK) {
			if (anIf->ifConfig)
				free_host_members(anIf->ifConfig);
			else
				anIf->ifConfig = (Host *)xmalloc(sizeof (Host));
			if (!anIf->ifConfig)
				return;
			memcpy(anIf->ifConfig, &th, sizeof (th));
			cancelWakeup(anIf->ifTimerID);
			if (anIf->ifState == DHCP_REQUESTING) {
				if (arp(anIf)) {
					if (++anIf->ifBadOffers ==
					    client.retries)
						fail(anIf, ERR_RETRIES);
					else
						reinit(anIf);
					return;
				} else
					initSuccess(anIf);
			} else if (anIf->ifState == DHCP_REBOOTING) {
				if (arp(anIf))
					reinit(anIf);
				else
					initSuccess(anIf);
			} else if (anIf->ifState == DHCP_RENEWING ||
			    anIf->ifState == DHCP_REBINDING) {
				renewComplete(anIf);
			}
			return;
		} else
			return;
#if DHCP_OVER_PPP
		break;
	default:
		informComplete(1, anIf);
		break;

#endif /* DHCP_OVER_PPP */
	}
}
