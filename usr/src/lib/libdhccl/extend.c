/*
 * extend.c: "Extend lease on an interface".
 *
 * SYNOPSIS
 *     void extend (IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *     Immediately try to renew the lease. If the prior state was
 *     "bound" a transition is made to "renewing". "renewing"
 *     and "rebinding" states remain unchanged.
 *
 * COPYRIGHT
 *     Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)extend.c 1.3 96/11/25 SMI"

#include "catype.h"
#include "dhcpstate.h"
#include "client.h"
#include "utils.h"

void
extend(IFINSTANCE *anIf)
{
	if (anIf->ifBusy)	/* already trying */
		return;

	cancelWakeup(anIf->ifTimerID);
	if (anIf->ifState == DHCP_BOUND) {
		renew(anIf);
	} else {
		if (checkAddr(anIf))
			return;
		retry(anIf, FALSE);
	}
}
