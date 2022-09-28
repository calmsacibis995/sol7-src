/*
 * timeout.c: "Handle expiration of a timer related to DHCP backoff and retry".
 *
 * SYNOPSIS
 *    void timeout(void *v)
 *    void retry(IFINSTANCE *anIf, int bdcast)
 *
 * DESCRIPTION
 *    This function is called whenever there is a DHCP transaction
 *    underway which times out. The action taken depends upon
 *    the state. A new packet may be sent, and a new timeout
 *    scheduled; the interface may be failed; or the policy
 *    may dictate the use of previously cached data.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)timeout.c 1.3 96/11/25 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "error.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "dhcpctrl.h"
#include "catype.h"
#include "unixgen.h"


void
retry(IFINSTANCE *anIf, int bdcast)
{
	int rc;

	anIf->ifSent++;
	rc = sendBp(anIf, bdcast);
	if (rc)
		fail(anIf, rc);
	else
		anIf->ifTimerID = schedule(client.timeouts[anIf->ifRetries],
		    timeout, anIf, &client.blockDuringTimeout);
}

void
timeout(void *v)
{
	int rc;
	IFINSTANCE *anIf = (IFINSTANCE *)v;

	switch (anIf->ifState) {
#if DHCP_OVER_PPP
	case 0:
		/* Inform timed out - failed */
		informComplete(0, anIf);
		return;
#endif /* DHCP_OVER_PPP */
	case DHCP_SELECTING:
		if (anIf->ifOffers > 0) {
#if ARP_AFTER_OFFER
		if (!arp(anIf))
			request(anIf);
		else {
			if (++anIf->ifBadOffers == client.retries)
				fail(anIf, ERR_RETRIES);
			else
				init(anIf);
		}
#else
			request(anIf);
#endif
		}
#if EMPLOY_BOOTP
		else if (anIf->ifBootp != 0) {
			anIf->ifConfig = anIf->ifBootp;
			anIf->ifBootp = 0;
			if (!arp(anIf))
				bootpComplete(anIf);
			else {
				free_host(anIf->ifBootp);
				anIf->ifBootp = 0;
				if (++anIf->ifBadOffers == client.retries)
					fail(anIf, ERR_RETRIES);
				else
					init(anIf);
			}
			return;
		}
#endif
		else {
			logw(DHCPCMSG39, anIf->ifName, DHCPmessageTypeString(
			    anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1),
			    client.timeouts[anIf->ifRetries]);
			if (client.timeouts[++anIf->ifRetries] > 0) {
				retry(anIf, TRUE);
			} else if ((anIf->ctrl_flags & CP_RETRY_MODES) ==
			    CP_FAIL_IF_TIMEOUT) {
				initFailure(anIf);
				fail(anIf, ERR_TIMED_OUT);
			} else if (client.timeouts[anIf->ifRetries] < 0) {
				anIf->ifTimerID = schedule(
				    -client.timeouts[anIf->ifRetries],
				    (void (*)(void *))init, anIf,
				    &client.blockDuringTimeout);
				anIf->ifXid = 0;
				anIf->ifRetries = -1;
				anIf->ifBusy = FALSE;
			} else {
				anIf->ifRetries--;
				retry(anIf, TRUE);
			}
		}
		return;
	case DHCP_REBOOTING:
		logw(DHCPCMSG39, anIf->ifName, DHCPmessageTypeString(
		    anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1),
		    client.timeouts[anIf->ifRetries]);
		if (client.timeouts[++anIf->ifRetries] > 0) {
			retry(anIf, TRUE);
		} else if (client.useNonVolatile && anIf->ifOldConfig) {
			/* so InitComplete sees old config */
			anIf->ifConfig = anIf->ifOldConfig;
			anIf->ifOldConfig = 0;
			initSuccess(anIf);
		} else
			init(anIf);
		return;
	case DHCP_REQUESTING:
		logw(DHCPCMSG39, anIf->ifName, DHCPmessageTypeString(
		    anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1),
		    client.timeouts[anIf->ifRetries]);
		if (client.timeouts[++anIf->ifRetries] > 0)
			retry(anIf, TRUE);
		else {
			if (++anIf->ifBadOffers == client.retries)
				fail(anIf, ERR_RETRIES);
			else
				init(anIf);
		}
		return;
	case DHCP_RENEWING:
		if (anIf->ifRetries < 0) {
			if (checkAddr(anIf) != 0)
				return;
			rc = clientSocket(TRUE, &anIf->ifOldConfig->Yiaddr);
			if (rc < 0) {
				fail(anIf, rc);
				return;
			}
			anIf->fd = rc;
			if (enableSigpoll(anIf->fd)) {
				fail(anIf, ERR_SETPOLL);
				return;
			}
			anIf->ifBusy = TRUE;
		}
		if (client.timeouts[++anIf->ifRetries] > 0)
			retry(anIf, FALSE);
		else {
			loge(DHCPCMSG63, anIf->ifName);
			anIf->ifXid = mrand48();
			anIf->ifBusy = FALSE;
			anIf->ifRetries = -1;
			close(anIf->fd);
			anIf->fd = -1;
			setNewWakeupTime(anIf);
		}
		return;
	case DHCP_REBINDING:
		if (anIf->ifRetries < 0) {
			if (checkAddr(anIf) != 0)
				return;
			rc = liOpen(anIf->ifName, &anIf->lli);
			if (rc) {
				fail(anIf, rc);
				return;
			}
			anIf->ifBusy = TRUE;
		}
		if (client.timeouts[++anIf->ifRetries] > 0)
			retry(anIf, TRUE);
		else {
			loge(DHCPCMSG64, anIf->ifName);
			anIf->ifXid = mrand48();
			anIf->ifBusy = FALSE;
			anIf->ifRetries = -1;
			liClose(&anIf->lli);
			setNewWakeupTime(anIf);
		}
		return;
	}
}
