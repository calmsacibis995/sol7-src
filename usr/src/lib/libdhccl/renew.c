/*
 * renew.c: "Make transitions to DHCP renewing state".
 *
 * SYNOPSIS
 * void renew(IFINSTANCE *anIf)
 * void renewComplete(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 * renew:
 * Called when the client makes a transition from "bound"
 * to "renewing". This occurs when T1 expires. Once
 * T1 has expired the client remains in the "renewing"
 * state until
 *     (a) the renewal is granted, whereupon it moves
 *         back to "bound"
 *     (b) the T2 expires whereupon it moves to "rebinding"
 *     (c) The lease expires (T2 unset or too close to
 *         expiration) when it moved to "discovering".
 *
 * renewComplete:
 *  Called when the client makes a transition from "renewing"
 *  or "rebinding" to "bound".
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)renew.c 1.3 96/11/25 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "dccommon.h"
#include "error.h"
#include "ca_time.h"
#include "catype.h"
#include "unixgen.h"
#include "hostdefs.h"

#if DEBUG
static char *dbg[] = {
/* 00 */ "%s T0=%ld T1=%ld T2=%ld granted=%ld=%s wake=%ld=%s state=%s\n",
0
};
#endif

void
renewComplete(IFINSTANCE *anIf)
{
	dhcptimer *t = &anIf->ifTimer;

	setT1T2(anIf->ifConfig);
	toTimer(t, anIf->ifConfig);
	fromTimer(t, anIf->ifOldConfig);
	setExpiration(anIf->ifOldConfig);
	if (!client.DontConfigure)
		write_dhc(client.dhcpconfigdir, anIf->ifName,
		    anIf->ifOldConfig);
	free_host(anIf->ifConfig);
	anIf->ifBusy = FALSE;
	anIf->ifConfig = 0;
	if (anIf->ifState == DHCP_REBINDING)
		liClose(&anIf->lli);
	else {
		close(anIf->fd);
		anIf->fd = -1;
	}
	anIf->ifState = DHCP_BOUND;
	setNewWakeupTime(anIf);
#if DEBUG
	if (debug >= 3) {
		struct tm *tm;
		char bufa[64], bufb[64];
		tm = localtime(&t->granted);
		strftime(bufa, sizeof (bufa), "%m/%d/%Y %R", tm);
		tm = localtime(&t->wakeup);
		strftime(bufb, sizeof (bufb), "%m/%d/%Y %R", tm);
		logb("renew/rebind succeeded\n");
		logb(dbg[0], anIf->ifName, (long)t->T0, (long)t->T1,
		    (long)t->T2, (long)t->granted, bufa, (long)t->wakeup, bufb,
		    ifStateString(anIf->ifState));
	}
#endif
}

void
renew(IFINSTANCE *anIf)
{
	int rc;

	if (checkAddr(anIf))
		return;

	anIf->ifState = DHCP_RENEWING;
	anIf->ifRetries = 0;
	anIf->ifReceived = 0;
	anIf->ifSent = 0;
	anIf->ifBadOffers = 0;
	anIf->ifXid = mrand48();
	prepareRenew(anIf);
	rc = clientSocket(TRUE, &anIf->ifOldConfig->Yiaddr);
	if (rc  < 0) {
		fail(anIf, rc);
		return;
	}
	anIf->fd = rc;
	if (enableSigpoll(anIf->fd)) {
		fail(anIf, ERR_SETPOLL);
		return;
	}
	retry(anIf, FALSE);
}
