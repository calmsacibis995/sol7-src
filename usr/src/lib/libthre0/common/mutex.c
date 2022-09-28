/*	Copyright (c) 1996-1998 by Sun Microsystems, Inc.	*/
/*	All rights reserved.					*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)mutex.c	1.62	98/01/16 SMI"

#ifdef __STDC__

#pragma weak mutex_init = _mutex_init
#pragma weak mutex_destroy = _mutex_destroy
#pragma weak mutex_lock = _mutex_lock
#pragma weak mutex_unlock = _mutex_unlock
#pragma weak mutex_trylock = _mutex_trylock

#pragma weak pthread_mutex_destroy = _mutex_destroy
#pragma weak pthread_mutex_lock = _pthread_mutex_lock
#pragma weak pthread_mutex_unlock = _pthread_mutex_unlock
#pragma weak pthread_mutex_trylock = _pthread_mutex_trylock
#pragma weak _pthread_mutex_destroy = _mutex_destroy

#pragma	weak _ti_mutex_lock = _mutex_lock
#pragma weak _ti_pthread_mutex_lock = _pthread_mutex_lock
#pragma	weak _ti_mutex_unlock = _mutex_unlock
#pragma	weak _ti_pthread_mutex_unlock = _pthread_mutex_unlock
#pragma weak _ti_mutex_destroy = _mutex_destroy
#pragma weak _ti_pthread_mutex_destroy = _mutex_destroy
#pragma	weak _ti_mutex_held = _mutex_held
#pragma	weak _ti_mutex_init = _mutex_init
#pragma	weak _ti_mutex_trylock = _mutex_trylock
#pragma	weak _ti_pthread_mutex_trylock = _pthread_mutex_trylock

#endif /* __STDC__ */

#include "libthread.h"
#include "tdb_agent.h"

static	void _mutex_adaptive_lock();
static	void _mutex_adaptive_unlock();
static	void _mutex_lwp_lock();
static	void _mutex_lwp_unlock();

static int _mutex_lock_robust(mutex_t *);
static int _mutex_trylock_robust(mutex_t *);
static int _mutex_unlock_robust(mutex_t *);

extern	int _mutex_unlock_asm();

struct thread *_lock_owner(mutex_t *);

/*
 * Check if a certain mutex is locked.
 */
int
_mutex_held(mutex_t *mp)
{
	return (LOCK_HELD(&mp->mutex_lockw));
}

/*ARGSUSED2*/
int
_mutex_init(mutex_t *mp, int type, void	*arg)
{
	int rc = 0;

	if (type != USYNC_THREAD && type != USYNC_PROCESS && !ROBUSTLOCK(type))
		return (EINVAL);
	if (ROBUSTLOCK(type)) {
		/*
		 * Do this in kernel.
		 * Register the USYNC_PROCESS_ROBUST mutex with the process.
		 */
		rc = ___lwp_mutex_init(mp, type);
	} else {
		mp->mutex_type = type;
		mp->mutex_magic = MUTEX_MAGIC;
		mp->mutex_waiters = 0;
		mp->mutex_flag = 0;
		mp->mutex_flag |= LOCK_INITED;
		_lock_clear_adaptive(mp);
	}
	if (!rc && __tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t) mp, MUTEX_MAGIC);
	return (rc);

}

void
_mutex_set_typeattr(mutex_t *mp, int attr)
{
	mp->mutex_type |= (uint8_t) attr;
}

/*ARGSUSED*/
int
_mutex_destroy(mutex_t *mp)
{
	mp->mutex_magic = 0;
	mp->mutex_flag &= ~LOCK_INITED;
	_tdb_sync_obj_deregister((caddr_t) mp);
	return (0);
}

int
_mutex_lock(mutex_t *mp)
{
	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_lock_robust(mp));
	}
	if (!_lock_try_adaptive(mp)) {
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_lock(mp);
		else
			_mutex_adaptive_lock(mp);
	}
	return (0);
}

int
_mutex_unlock(mutex_t *mp)
{
	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_unlock_robust(mp));
	}
	if (_mutex_unlock_asm(mp) > 0) {
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_unlock(mp);
		else
			_mutex_adaptive_unlock(mp);
	}
	return (0);
}

int
_mutex_trylock(mutex_t *mp)
{

	if (ROBUSTLOCK(mp->mutex_type)) {
		return (_mutex_trylock_robust(mp));
	}
	if (_lock_try_adaptive(mp)) {
		return (0);
	} else {
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf->eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
		return (EBUSY);
	}
}

void
_mutex_sema_unlock(mutex_t *mp)
{
	u_char waiters;

	ASSERT(curthread->t_nosig >= 2);
	if (_mutex_unlock_asm(mp) > 0) {
		if (_t_release((caddr_t)mp, &waiters, 0) > 0)
			mp->mutex_waiters = waiters;
	}
	_sigon();
}

#define	MUTEX_MAX_SPIN	100		/* how long to spin before blocking */
#define	MUTEX_CHECK_FREQ 10		/* frequency of checking lwp state */

static void
_mutex_adaptive_lock(mutex_t *mp)
{
	u_char waiters;
	struct thread *owner_t;
	short state;
	int spin_count = 0;

	while (!_lock_try_adaptive(mp)) {
		/* only check owner every so often, including first time */
		if ((spin_count++) % MUTEX_CHECK_FREQ == 0) {
			_sched_lock();
			mp->mutex_magic = MUTEX_MAGIC;
			if (__tdb_attach_stat != TDB_NOT_ATTACHED)
				_tdb_sync_obj_register((caddr_t) mp,
				    MUTEX_MAGIC);
			/* look at (real) onproc state of owner */
			owner_t = _lock_owner(mp);
			if (spin_count < MUTEX_MAX_SPIN && owner_t != NULL) {
				if (owner_t->t_state == TS_ONPROC &&
				    owner_t->t_lwpdata != NULL) {
					state = owner_t->t_lwpdata->sc_state;
					if (state == SC_ONPROC) {
						/*
						 * thread and lwp are ONPROC,
						 * so spin for a while
						 */
						_sched_unlock();
						continue;
					}
				}
			}
			/* may be a long wait, better block */
			waiters = mp->mutex_waiters;
			mp->mutex_waiters = 1;
			if (!_lock_try_adaptive(mp)) {
				curthread->t_flag &= ~T_WAITCV;
				_t_block((caddr_t)mp);
				_sched_unlock_nosig();
				_swtch(0);
				_sigon();
				spin_count = 0;
				continue;
			}
			mp->mutex_waiters = waiters;	/* got the lock */
			_sched_unlock();
			return;
		}
	}
}

static void
_mutex_adaptive_unlock(mutex_t *mp)
{
	u_char waiters;

	_sched_lock();
	if (_t_release((caddr_t)mp, &waiters, 0) > 0)
		mp->mutex_waiters = waiters;
	_sched_unlock();
}

static void
_mutex_lwp_lock(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t) mp, MUTEX_MAGIC);
	_lwp_mutex_lock(mp);

	/* for libthread_db's benefit */
	*(uthread_t **) &mp->mutex_owner = curthread;
}

static void
_mutex_lwp_unlock(mutex_t *mp)
{
	/*
	 * Wakeup a waiter, if any.
	 */
	___lwp_mutex_wakeup(mp);
}

/*
 * this is the _mutex_lock() routine that is used internally
 * by the thread's library.
 */
void
_lmutex_lock(mutex_t *mp)
{
	_sigoff();
	_mutex_lock(mp);
}

/*
 * this is the _mutex_trylock() that is used internally by the
 * thread's library.
 */
int
_lmutex_trylock(mutex_t *mp)
{
	_sigoff();
	if (_lock_try_adaptive(mp)) {
		_sigon();
		return (1);
	}
	return (0);
}

/*
 * this is the _mutex_unlock() that is used internally by the
 * thread's library.
 */
void
_lmutex_unlock(mutex_t *mp)
{
	_mutex_unlock(mp);
	_sigon();
}

int
_pthread_mutex_lock(mutex_t *mp)
{
	if (!_lock_try_adaptive(mp)) {
		if (_lock_owner(mp) == curthread) {
			if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
				if (mp->mutex_rcount == RECURSION_MAX)
					return (EAGAIN);
				mp->mutex_rcount++;
				return (0);
			} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
				return (EDEADLOCK);
			}
		}
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_lock(mp);
		else
			_mutex_adaptive_lock(mp);
	}
	/* first acquistion of a recursive lock */
	if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE)
		mp->mutex_rcount++;
	return (0);
}

int
_pthread_mutex_unlock(mutex_t *mp)
{
	if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
		if (_lock_owner(mp) == curthread) {
			if (--mp->mutex_rcount)
				return (0);
		} else {
				return (EPERM);
		}
	} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
		if (_lock_owner(mp) != curthread)
			return (EPERM);
	}
	if (_mutex_unlock_asm(mp) > 0) {
		if (mp->mutex_type & USYNC_PROCESS)
			_mutex_lwp_unlock(mp);
		else
			_mutex_adaptive_unlock(mp);
	}
	return (0);
}

int
_pthread_mutex_trylock(mutex_t *mp)
{
	if (_lock_try_adaptive(mp)) {
		if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE)
			mp->mutex_rcount++;
		return (0);
	} else {
		if (_lock_owner(mp) == curthread) {
			if (mp->mutex_type & PTHREAD_MUTEX_RECURSIVE) {
				if (mp->mutex_rcount == RECURSION_MAX)
					return (EAGAIN);
				mp->mutex_rcount++;
				return (0);
			} else if (mp->mutex_type & PTHREAD_MUTEX_ERRORCHECK) {
				return (EDEADLOCK);
			}
		}
		if (__td_event_report(curthread, TD_LOCK_TRY)) {
			curthread->t_td_evbuf->eventnum = TD_LOCK_TRY;
			tdb_event_lock_try();
		}
		return (EBUSY);
	}
}

static int
_mutex_lock_robust(mutex_t *mp)
{
	mp->mutex_magic = MUTEX_MAGIC;
	if (__tdb_attach_stat != TDB_NOT_ATTACHED)
		_tdb_sync_obj_register((caddr_t) mp, MUTEX_MAGIC);
	return (___lwp_mutex_lock(mp));
}

static int
_mutex_unlock_robust(mutex_t *mp)
{
	return (___lwp_mutex_unlock(mp));
}

static int
_mutex_trylock_robust(mutex_t *mp)
{
	return (___lwp_mutex_trylock(mp));
}
