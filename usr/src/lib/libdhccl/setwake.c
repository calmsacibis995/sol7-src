/*
 * setwake.c: "Compute new wakeup time for DHCP agent on an interface".
 *
 * SYNOPSIS
 *    void setNewWakeupTime(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    This functions sets the wakeup time for renewing, rebinding
 *    or expiring the DHCP lease. The code is a little bit messy
 *    because of the DHCP convention for infinite leases (-1)
 *    and because of the possibility of overflow in performing
 *    integer arithmetic on data that are time offsets from
 *    the UNIX "zero" time of 01/01/1970 00:00.
 *
 * BUGS
 *    The time values may wrap around on very long leases
 *    or when one approaches the year 2038. By then I
 *    doubt that anyone will care.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)setwake.c 1.3 96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "msgindex.h"
#include "ca_time.h"
#include <values.h>
#include "unixgen.h"

void
setNewWakeupTime(IFINSTANCE *anIf)
{
	time_t compareTime, now;
	dhcptimer *t = &anIf->ifTimer;

	t->wakeup = MAXLONG;

	if (t->T0 < 0)
		return;

	compareTime = t->granted + t->T0;
	if (compareTime < t->granted)
		return;  /* XXXX ##BUGS## See BUGS above */

	now = GetCurrentSecond();

	if (now >= compareTime) {
		expireIf(anIf);
		return;
	}

	compareTime = t->granted + t->T2;
	if (compareTime < t->granted)
		return; /* See BUGS */

	if (now >= compareTime) {
		t->wakeup = (now + t->granted + t->T0) / 2;
		if (t->wakeup < now)
			return; /* See BUGS */

		if (t->granted + t->T0 - t->wakeup < client.timewindow) {
			t->wakeup = t->granted + t->T0;
			loge(DHCPCMSG61, anIf->ifName, t->wakeup - now);
			anIf->ifTimerID = schedule(t->wakeup - now,
			    (void (*)(void *))expireIf, anIf,
			    &client.blockDuringTimeout);
			return;
		}
		anIf->ifTimerID = schedule(t->wakeup - now, timeout, anIf,
		    &client.blockDuringTimeout);
		return;
	}

	compareTime = t->granted + t->T1;
	if (compareTime < t->granted)
		return; /* See BUGS */

	if (now >= compareTime) {
		t->wakeup = (now + t->granted + t->T2) / 2;
		if (t->wakeup < now)
			return; /* See BUGS */
		if (t->granted + t->T2 - t->wakeup < client.timewindow) {
			t->wakeup = t->granted + t->T2;
			anIf->ifTimerID = schedule(t->wakeup - now,
			    (void (*)(void *))rebind, anIf,
			    &client.blockDuringTimeout);
			return;
		}
		anIf->ifTimerID = schedule(t->wakeup - now, timeout, anIf,
		    &client.blockDuringTimeout);
		return;
	}

	t->wakeup = t->granted + t->T1;
	anIf->ifTimerID = schedule(t->wakeup - now, (void (*)(void *))renew,
	    anIf, &client.blockDuringTimeout);
}
