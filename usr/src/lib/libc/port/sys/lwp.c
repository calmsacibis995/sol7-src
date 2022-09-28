/*
 * Copyright (c) 1992, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)lwp.c	1.18	97/12/23 SMI"

/*LINTLIBRARY*/

#pragma weak _lwp_mutex_lock = __lwp_mutex_lock
#pragma weak _lwp_mutex_trylock = __lwp_mutex_trylock
#pragma weak _lwp_sema_init = __lwp_sema_init

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <synch.h>
#include <sys/synch32.h>

#if defined(__sparcv9) || defined(sparcv9)
extern int _lock_try(unsigned long *);
#else
extern int _lock_try(unsigned char *);
#endif /* __sparcv9 */

int
_lwp_mutex_lock(mutex_t *mp)
{
	if (!_lock_try(&mp->mutex_lockw)) {
		/*
		 * Cannot afford a jump into the run-time linker at this point.
		 * This is for a very specific, machine dependent reason.
		 * The scenario is on x86: _lwp_terminate(), in libthread's
		 * x86 specific portion, calls __freegs(), which calls
		 * __free_selector(), which calls _lwp_mutex_lock(). At this
		 * point, %gs info is not available. If the linker is invoked,
		 * it could call into libthread, and try to dereference
		 * curthread, which would try to read non-existent %gs info
		 * and seg-fault.
		 * This bug was exposed by thr_join_011 in the threads test
		 * suite.
		 * So, get a pointer to ___lwp_mutex_lock() instead, and call
		 * through the pointer - this circumvents the run-time linker.
		 * Since this is a kernel trap, the performance overhead is not
		 * significant.
		 * Also, see __freegs() and __free_selector() in libc/i386 which
		 * use the same hack to circumvent the linker.
		 */
		extern int ___lwp_mutex_lock(mutex_t *);
		int (*lockfunc)(mutex_t *) = &___lwp_mutex_lock;

		return ((*lockfunc)(mp));
	}
	return (0);
}

int
_lwp_mutex_trylock(mutex_t *mp)
{
	if (_lock_try(&mp->mutex_lockw)) {
		return (0);
	}
	return (EBUSY);
}

int
_lwp_sema_init(lwp_sema_t *sp, int count)
{
	sp->sema_count = count;
	sp->sema_waiters = 0;
	sp->type = USYNC_PROCESS;
	return (0);
}

void
_halt(void)
{
	/*LINTED*/
	while (1)
		continue;
}
