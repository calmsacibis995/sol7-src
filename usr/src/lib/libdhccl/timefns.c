/*
 * timefns.c: "Convert DHCP times to an from actual UCT values".
 *
 * SYNOPSIS
 *    void toTimer       (dhcptimer *t, const Host *host)
 *    void fromTimer     (const dhcptimer *t, Host *host)
 *    void setExpiration (Host *host)
 *    void setT1T2       (Host *host)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)timefns.c 1.2 96/11/21 SMI"

#include "client.h"
#include "hostdefs.h"
#include "ca_time.h"

void
toTimer(dhcptimer *t, const Host *host)
{
	t->T0 = host->stags[TAG_LEASETIME].dhcd4;
	t->T1 = host->stags[TAG_DHCP_T1].dhcd4;
	t->T2 = host->stags[TAG_DHCP_T2].dhcd4;
	t->granted = GetCurrentSecond();
}

void
fromTimer(const dhcptimer *t, Host *host)
{
	host->stags[TAG_LEASETIME].dhcd4 = t->T0;
	host->stags[TAG_DHCP_T1].dhcd4 = t->T1;
	host->stags[TAG_DHCP_T2].dhcd4 = t->T2;
}

void
setExpiration(Host *host)
{
	time_t expire;
	time_t current;

	/*
	 * Finite lease (if lease time <0 or adding to current time
	 * overflows then time will overflow so set expiration to
	 * infinity)
	 */

	if (host->stags[TAG_LEASETIME].dhcd4 != -1) {
		current = GetCurrentSecond();
		if ((current + host->stags[TAG_LEASETIME].dhcd4) < current)
			expire = INFINITY_TIME;
		else
			expire = current + host->stags[TAG_LEASETIME].dhcd4;
	} else
		expire = INFINITY_TIME;

	host->lease_expires = expire;
}

void
setT1T2(struct Host *host)
{
	if (!inhost(host->flags, TAG_DHCP_T1)) {
		onhost(host->flags, TAG_DHCP_T1);
		host->stags[TAG_DHCP_T1].dhcd4 =
		    0.5 * host->stags[TAG_LEASETIME].dhcd4;
	}

	if (!inhost(host->flags, TAG_DHCP_T2)) {
		onhost(host->flags, TAG_DHCP_T2);
		host->stags[TAG_DHCP_T2].dhcd4 = 0.875 *
		    host->stags[TAG_LEASETIME].dhcd4;
	}
}
