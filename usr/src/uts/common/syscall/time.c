/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)time.c	1.5	97/08/12 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/debug.h>

time_t
gtime(void)
{
	return (hrestime.tv_sec);
}

int
stime(time_t time)
{
	int s;
	timestruc_t ts;

	if (!suser(CRED()))
		return (set_errno(EPERM));

	mutex_enter(&tod_lock);
	ts.tv_sec = time;
	ts.tv_nsec = 0;
	tod_set(ts);
	s = hr_clock_lock();
	hrestime = ts;
	timedelta = 0;
	hr_clock_unlock(s);
	mutex_exit(&tod_lock);

	return (0);
}
