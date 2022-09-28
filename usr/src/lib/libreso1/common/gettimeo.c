/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef LINT
static char rcsid[] = "$Id: gettimeofday.c,v 8.3 1996/11/18 09:08:59 vixie Exp $";
#endif


#pragma ident   "@(#)gettimeofday.c 1.1     97/12/03 SMI"

#include "port_before.h"
#include "port_after.h"

#if !defined(NEED_GETTIMEOFDAY)
int __bindcompat_gettimeofday;
#else
int
gettimeofday(struct timeval *tvp, struct _TIMEZONE *tzp) {
	time_t clock, time(time_t *);

	if (time(&clock) == (time_t) -1)
		return (-1);
	if (tvp) {
		tvp->tv_sec = clock;
		tvp->tv_usec = 0;
	}
	if (tzp) {
		tzp->tz_minuteswest = 0;
		tzp->tz_dsttime = 0;
	}
	return (0);
}
#endif /*NEED_GETTIMEOFDAY*/
