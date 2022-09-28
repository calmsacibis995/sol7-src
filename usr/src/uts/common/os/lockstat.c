/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockstat_subr.c	1.1	97/04/01 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/time.h>
#include <sys/thread.h>
#include <sys/lockstat.h>

/*
 * Resident support for the lockstat driver.
 */

u_char lockstat_event[LS_MAX_EVENTS];

void (*lockstat_enter)(uintptr_t, uintptr_t, uintptr_t) =
	lockstat_enter_nop;
void (*lockstat_exit)(uintptr_t, uintptr_t, uint32_t, uintptr_t, uintptr_t) =
	lockstat_exit_nop;
void (*lockstat_record)(uintptr_t, uintptr_t, uint32_t, uintptr_t, hrtime_t) =
	lockstat_record_nop;

/* ARGSUSED */
void
lockstat_enter_nop(uintptr_t lp, uintptr_t caller, uintptr_t owner)
{
}

/* ARGSUSED */
void
lockstat_exit_nop(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, uintptr_t owner)
{
}

/* ARGSUSED */
void
lockstat_record_nop(uintptr_t lp, uintptr_t caller, uint32_t event,
	uintptr_t refcnt, hrtime_t duration)
{
}

int
lockstat_depth(void)
{
	return (curthread->t_lockstat);
}

int
lockstat_active_threads(void)
{
	kthread_t *tp;
	int active = 0;

	mutex_enter(&pidlock);
	tp = curthread;
	do {
		if (tp->t_lockstat)
			active++;
	} while ((tp = tp->t_next) != curthread);
	mutex_exit(&pidlock);
	return (active);
}
