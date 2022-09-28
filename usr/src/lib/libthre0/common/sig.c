/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sig.c	1.107	98/01/14 SMI"

#include "libthread.h"
#include "tdb_agent.h"
#include <sys/reg.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/*
 * Global variable
 */
sigset_t 	_cantmask;	/* initialized in _t0init() */
sigset_t 	_lcantmask;	/* set of signals libthread can't mask */
int		_sigwaitingset = 0;	/* tells when SIGWAITING is enabled */
thread_t	__dynamic_tid;		/* aslwp's tid */
lwpid_t		__dynamic_lwpid;	/* aslwp's lwpid */

/*
 * Static function
 */
static	void _resetsig(void);
static	void _bouncesigs(void);
static	void _dodynamiclwps(void);
static	int  _sigredirect_thread(uthread_t *, int, thread_t *, int, int);
static	void _sigsuspnd(uthread_t *);

/*
 * Static variables
 */
static	struct sigaction _acton =
	{SA_RESTART|SA_WAITSIG, _dodynamiclwps, 0};
static	struct sigaction _actoff = {0, SIG_IGN, 0};


void
_sigon(void)
{
	extern void __sendsig(int);
	extern void __bounceself(void);
	uthread_t *t = curthread;
	uthread_t *tt;

again:
	/*
	 * Bail out if nosig is on.
	 * If we're in the SIGLWP handler, bail out now because we'll handle
	 * any pending signals at the end of the handler instead.
	 */
	if (--t->t_nosig || (t->t_flag & T_INSIGLWP))
		return;
	if (t->t_preempt | t->t_pending | t->t_stop | t->t_bdirpend
	    | t->t_sig) {
		if (t->t_preempt | t->t_stop) {
			/*
			 * This thread could have been preempted while in a
			 * critical section (t->t_nosig > 0) or the thread was
			 * preempted while being dispatched.
			 */
			t->t_preempt = 0;
			/* disable preemption */
			t->t_nosig++;
			_sched_lock_nosig();
			_dopreempt();
			/* sched lock is unlocked by _dopreempt() */
			/*
			 * check if thread was preempted on its return
			 * from _dopreempt(). if it wasn't preempted, then
			 * preemption is enabled by decrementing t_nosig
			 * at the top of this loop.
			 */
			goto again;
		}
		/*
		 * If a signal was received while in the critical section, but
		 * which was blocked by the current thread, it needs to be
		 * re-directed to other threads now.
		 * Note that there could be more than one signal (recorded in
		 * _bpending) - this is because all signals are not blocked
		 * as soon as the first such signal is received as is done in
		 * the case of t_sig. Other threads also could have contributed.
		 */
		/*
		 * Handle stored signal first!
		 */

		if (t->t_sig) {
			/*
			 * The following two lines are CRUCIAL! Otherwise, the
			 * linker could be invoked on the call to __sendsig().
			 * The linker calls _sigon()/_sigoff() indirectly via
			 * the _lrw_lock()/unlock() calls and this would result
			 * in a recursive call which ends up here - unless t_sig
			 * is cleared before calling __sendsig().
			 */
			int sig = t->t_sig;
			t->t_sig = 0;
			__sendsig(sig);
		}
		if (t->t_pending)
			_resetsig();
		if (t->t_bdirpend)
			__bounceself();
	}
}

void
_sigoff(void)
{
	curthread->t_nosig++;
}

/*
 * _deliversigs() is used to deliver signals which were sent via
 * _thr_kill() but have not yet been received by the target thread
 * or those which are blocked in the process virtual signal mask and this
 * thread unblocked one of them.
 * Note that _deliversigs() will not result in "multiplication" of signals,
 * since signals received via lwp_kill() are matched with the t_ssig mask.
 * If the signal is not in the mask, it is ignored.
 */
void
_deliversigs(const sigset_t *nmask)
{
	sigset_t olwpmask;
	sigset_t resend;
	uthread_t *t = curthread;
	int sig;

	if (!sigisempty(&t->t_ssig)) {
		/* block all signals */
		__sigprocmask(SIG_SETMASK, &_totalmasked, &olwpmask);
		_sched_lock_nosig();
		if (_blocking(&t->t_ssig, &t->t_hold, (sigset_t *)nmask,
		    &resend)) {
			_sched_unlock_nosig();
			ASSERT(!sigcmpset(&olwpmask, &_totalmasked) ||
			    (!sigand(&olwpmask, &resend)));
			while (sig = _fsig(&resend)) {
				_sigdelset(&resend, sig);
				if (_lwp_kill(LWPID(t), sig) != 0)
					_panic("_deliversigs: _lwp_kill");
			}
		} else
			_sched_unlock_nosig();
		__sigprocmask(SIG_SETMASK, &olwpmask, NULL);
	}
}

void
__unsafe_bsigs(sigset_t *sigs)
{
	int sig;
	uthread_t *t = curthread;

	while (sig = _fsig(sigs)) {
		_sigdelset(sigs, sig);
		while (_lwp_sigredirect(LWPID(t), sig) == 0);
	}
}

void
_bsigs(sigset_t *sigs)
{
	sigset_t olwpmask;

	__sigprocmask(SIG_SETMASK, &_totalmasked, &olwpmask);
	ASSERT(!sigand(&olwpmask, sigs));
	__unsafe_bsigs(sigs);
	__sigprocmask(SIG_SETMASK, &olwpmask, NULL);
}

void
__bounceself(void)
{
	sigset_t olwpmask;
	sigset_t sigs;
	int sig;
	uthread_t *t = curthread;
	extern void _sigunblock(sigset_t *, sigset_t *, sigset_t *);

	__sigprocmask(SIG_SETMASK, &_totalmasked, &olwpmask);
	if (sigcmpset(&_totalmasked, &olwpmask) == 0)
		return;
	if (t->t_bdirpend) {
		sigset_t tset;

		_sched_lock_nosig();
		maskreverse(&t->t_hold, &tset);
		sigandset(&tset, &_ignoredset, &tset);
		sigandset(&tset, &t->t_bsig, &tset);
		sigdiffset(&t->t_bsig, &tset);
		while (sig = _fsig(&tset)) {
			_sigdelset(&tset, sig);
			_lwp_sigredirect(0, sig);
		}
		_sigunblock(&t->t_bsig, &t->t_hold, &sigs);
		if (!sigisempty(&sigs)) {
			t->t_bdirpend = !sigisempty(&t->t_bsig);
			if (t->t_bdirpend == 0)
				t->t_flag &= ~T_BSSIG;
			_sched_unlock_nosig();
			__unsafe_bsigs(&sigs);
			ASSERT(!sigand(&olwpmask, &sigs));
		} else
			_sched_unlock_nosig();
	}
	__sigprocmask(SIG_SETMASK, &olwpmask, NULL);
}

/*
 * redispatch the set of currently unblocked signals to the active
 * thread. signals are masked while resending the signals but are
 * delivered when the thread's mask is restored.
 */
static void
_resetsig(void)
{
	uthread_t *t = curthread;
	int sig;
	sigset_t signalled;
	sigset_t psig;
	sigset_t olwpmask;

	__sigprocmask(SIG_SETMASK, &_totalmasked, &olwpmask);
	/*
	 * When all signals are already masked in this LWP
	 * (olwpmask == _totalmasked), no pending signals should
	 * be resent. This is a hack that prevents the linker from
	 * calling _resetsig() recursively. It shall be removed
	 * when the library is rebuilt with -Bsymbolic.
	 */
	if (sigcmpset(&_totalmasked, &olwpmask) == 0)
		return;
	/*
	 * t_pending could be zero here since the corresponding
	 * signal could have occurred in the window between the
	 * check for t_pending and the call to _resetsig() in _sigon().
	 */
	if (t->t_pending) {
		sigset_t tset;

		_sched_lock_nosig();
		maskreverse(&t->t_hold, &tset);
		sigandset(&tset, &_ignoredset, &tset);
		sigdiffset(&t->t_psig, &tset);
		sigdiffset(&t->t_ssig, &tset);
		_sigmaskset(&t->t_psig, &t->t_hold, &psig);
		t->t_pending = !sigisempty(&t->t_psig);
		_sched_unlock_nosig();

		signalled = psig;
		while (sig = _fsig(&psig)) {
			_sigdelset(&psig, sig);
			if (_lwp_kill(LWPID(t), sig) < 0)
				_panic("_resetsig: _lwp_kill");
		}
		_sched_lock_nosig();
		sigorset(&t->t_ssig, &signalled);
		ASSERT(sigisempty(&signalled) || !sigisempty(&t->t_ssig));
		ASSERT((t->t_flag & T_TEMPBOUND) ||
		    !sigand(&olwpmask, &signalled));
		_sched_unlock_nosig();
	}
	__sigprocmask(SIG_SETMASK, &olwpmask, NULL);
}

/*
 * The following depends on the HASH_TID function to be (tid % ALLTHR_TBLSIZ).
 * If this function changes in the future, the traversal of the allthreads
 * list below has to be always from 1 to (ALLTHR_TBLSIZ - 1). Then you
 * may not be able to optimize such that the search can stop at maxtid as
 * is being done below. Returns 0 on success and 1 on failure.
 */
int
_sigredirect(int sig)
{

	int i, maxtid;
	int firstpass = 1;
	uthread_t *first, *t;
	thread_t savedtid = 0;
	lwpid_t lwpid;


	/*
	 * Called with all signals blocked.
	 * First record the signal to be bounced in the virtual process mask.
	 * This is important and has to be done before reading _lasttid.
	 */

	_lwp_mutex_lock(&_pmasklock);
	dbg_sigaddset(&_tpmask, sig);
	_sigaddset(&_pmask, sig);
	_lwp_mutex_unlock(&_pmasklock);

	_lmutex_lock(&_tidlock);
	maxtid = _lasttid;
	_lmutex_unlock(&_tidlock);

	if (maxtid >= ALLTHR_TBLSIZ)
		maxtid = ALLTHR_TBLSIZ - 1;
	/*
	 * Now start the search for a candidate recipient thread.
	 * Note that it is OK to NOT hold _schedlock over the whole search
	 * to prevent threads from changing their signal masks.
	 * Reason this algorithm works:
	 * Since sig has already been recorded above, if a thread which has
	 * been passed over in the search, now unblocks sig, it will get the
	 * signal. Now, when and if a candidate is found, check if the sig is
	 * still in the _pmask. If not, you are done, since this means some
	 * other thread self-delivered it. If yes, then send the sig to the
	 * candidate thread which is guaranteed to receive it. If a candidate
	 * thread is not found, just return.
	 */
_restartbkt:
	for (i = 1; i <= maxtid; i++) {
		_lock_bucket(i);
		if ((first = _allthreads[i].first) == NULL) {
			_unlock_bucket(i);
			continue;
		}
		_sched_lock_nosig();
		t = first;
		do {
			if (_sigredirect_thread(t, sig, &savedtid,
			    firstpass, i)) {
				continue;
			} else {
				_sched_unlock_nosig();
				_unlock_bucket(i);
				return (1); /* success */
			}
		} while ((t = t->t_next) != first);
		_sched_unlock_nosig();
		_unlock_bucket(i);
		/*
		 * Quick check to see if some other thread delivered sig.
		 */
		_lwp_mutex_lock(&_pmasklock);
		if (!_sigismember(&_pmask, sig)) {
			dbg_delset(&_tpmask, sig);
			_lwp_mutex_unlock(&_pmasklock);
			return (1); /* success */
		}
		_lwp_mutex_unlock(&_pmasklock);
	}
	if (savedtid && firstpass) {
		int ix;

		firstpass = 0;
		_lock_bucket((ix = HASH_TID(savedtid)));
		if ((t = _idtot(savedtid)) == (uthread_t *) -1) {
			_unlock_bucket(ix);
			goto _restartbkt;
		}
		_sched_lock_nosig();
		if (_sigredirect_thread(t, sig, &savedtid, firstpass, ix)) {
			_sched_unlock_nosig();
			_unlock_bucket(ix);
			goto _restartbkt;
		} else {
			_sched_unlock_nosig();
			_unlock_bucket(ix);
			return (1);
		}
	}
	_lwp_mutex_lock(&_pmasklock);
	dbg_delset(&_tpmask, sig);
	_lwp_mutex_unlock(&_pmasklock);
	_cond_broadcast(&_sigwait_cv);
	return (0); /* failed to find a thread to send the signal to */
}

/*
 * _sigsuspnd: SIGSUSPND handler
 */
static void
_sigsuspnd(uthread_t *t)
{
	ASSERT(LOCK_HELD(&_schedlock.mutex_lockw));
	ASSERT(curthread->t_state != TS_SLEEP);
	_dopreempt();
	/*
	 * Turn off T_INSIGLWP flag to allow _sigon() to handle any
	 * pending signals before we leave the handler.
	 */
	t->t_flag &= ~T_INSIGLWP;
	_sigon();
	/*
	 * signals are left disabled on return from _dopreempt().
	 * _sigon() checks if a signal was sent to this thread
	 * when it had signals disabled.
	 */
}

/*
 * _siglwp: SIGLWP handler
 */
void
_siglwp(int sig, siginfo_t *sip, ucontext_t *uap)
{
	uthread_t *t = curthread;

	_sched_lock();
	ASSERT(_sigismember(&curthread->t_hold, SIGLWP));
	if (t->t_stop) {
		_sigsuspnd(t);
		return;
	} else if (PREEMPTED(t)) {
		ASSERT(curthread->t_state != TS_SLEEP);
		if (ISBOUND(t))
			_panic("_siglwp: preempting bound thread");
		_dopreempt();
		/*
		 * Turn off T_INSIGLWP flag to allow _sigon() to handle any
		 * pending signals before we leave the handler.
		 */
		t->t_flag &= ~T_INSIGLWP;
		_sigon();
		/*
		 * signals are left disabled on return from _dopreempt().
		 * _sigon() checks if a signal was sent to this thread
		 * when it had signals disabled.
		 */
		return;
	}
	_sched_unlock();
}

/*
 * A dummy signal handler that enables the dynamiclwps() thread
 * to catch SIGWAITING signals. This routine will never be called
 * because the dynamiclwps() thread synchronously waits for a SIGWAITING
 * signal by doing a sigwait(). This routine enables the process to
 * catch SIGWAITING by registering a handle with sigaction(). A SIGWAITING
 * signal, however, will always be sent to the dynamiclwps() thread because
 * all threads except the dynamiclwps() thread have SIGWAITING  blocked.
 */
static void
_dodynamiclwps(void)
{}
static char dbuf[64];

/*
 * This is the thread that blocks waiting to receive SIGWAITING
 * signals when SIGWAITING is enabled.
 */
void
_dynamiclwps(void)
{
	int sig = 0;
	struct timespec ts;

	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	while (1) {
		/* See if we need to do anything for libthread_db. */
		_tdb_agent_check();
		sig = _signotifywait();
		if (sig == SIGWAITING) {
			/* Discard the signal */
			_lwp_sigredirect(0, SIGWAITING);
			_sched_lock_nosig();
			if (!_nrunnable ||(_nrunnable - _nidlecnt) <= _naging) {
				/* disable sigwaiting */
				if (_sigwaitingset) {
					_sigwaitingset = 0;
					__sigaction(SIGWAITING, &_actoff, NULL);
				}
				_sched_unlock_nosig();
			} else {
				_sched_unlock_nosig();
				if (_new_lwp(NULL, _age, 0))
					_panic("cannot create new lwp");
			}
		} else if (sig == SIGLWP) {
			/*
			 * This was a signal from libthread_db.  We check
			 * whether libthread_db needs us to do anything
			 * on each loop iteration.  Then discard the
			 * signal.
			 */
			_lwp_sigredirect(0, sig);
			continue;
		} else if (sig > 0) {
			ASSERT(!_sigismember(&_pmask, sig));
			_sigredirect(sig);
		} else if (sig != -1 ||
		    (errno != EINTR && errno != EACCES && errno != EAGAIN)) {
			_panic("_dynamiclwps(): bad return: _signotifywait()");
		}
	}
}

/*
 * enable the SIGWAITING handler. this only happens when there are
 * more runnable threads than there are lwps. SIGWAITING notifies the
 * thread library to allocate more lwps because the current set of lwps
 * are blocked on system calls.
 */
void
_sigwaiting_enabled(void)
{

	ASSERT(_sigwaitingset == 0);
	__sigaction(SIGWAITING, &_acton, NULL);
	_sigwaitingset = 1;
}

/*
 * disable the SIGWAITING handler. this happens when there are no more
 * runnable threads than there are lwps.
 */
void
_sigwaiting_disabled(void)
{

	ASSERT(_sigwaitingset == 1);
	__sigaction(SIGWAITING, &_actoff, NULL);
	_sigwaitingset = 0;
}

/*
 * Create a bound, daemon, libthread-internal thread.  Currently,
 * there are (up to) 2 of these:  the "dynamic" thread, which
 * creates new LWP's in response to a SIGWAITING signal; and the
 * libthread_db agent thread, which should only be created when
 * a process is being manipulated by libthread_db.
 */
void
_sys_thread_create(void (*entry)(void), int flags)
{
	uthread_t *t;
	caddr_t stk, tls;
	char buf[64];
	int ret;
	int ix;
	uthread_t *first;

	/*
	 * XXX: Use big, default stacks for now, since the dynamic linker may
	 * use a huge stack. When this linker bug gets fixed, revert back to
	 * using smaller stacks for daemon threads like the sigwaiting lwp.
	 */
	if (!_alloc_thread(NULL, 0, &t, _lpagesize)) {
		_panic("_sys_thread_create():alloc_thread returns 0 (no mem)");
	}
	/*
	 * do this only if you want this lwp to appear as a thread
	 * to the user.
	 */
	_mutex_lock(&_tidlock);
	t->t_tid = (thread_t) ++_lasttid;
	_mutex_unlock(&_tidlock);
	ix = HASH_TID(t->t_tid);
	_mutex_lock(&(_allthreads[ix].lock));
	if ((first = _allthreads[ix].first) == NULL)
		_allthreads[ix].first = t->t_next =
		    t->t_prev = t;
	else {
		t->t_prev = first->t_prev;
		t->t_next = first;
		first->t_prev->t_next = t;
		first->t_prev = t;
	}
	_mutex_unlock(&(_allthreads[ix].lock));
	masktotalsigs(&t->t_hold);
	t->t_usropts = THR_BOUND | THR_DETACHED | THR_DAEMON;
	t->t_flag |= T_INTERNAL;
	_lwp_sema_init(&t->t_park, NULL);
	t->t_park.sema_type = USYNC_THREAD;

	t->t_state = TS_ONPROC;
	ret = _lwp_exec(t, (uintptr_t)_thr_exit, (caddr_t)t->t_sp, entry,
	    flags, &LWPID(t));
	if (ret) {
		sprintf(buf, "_sys_thread_create() failed, errno = %d\n", ret);
		_write(1, buf, strlen(buf));
		exit(1);
	}
	if (flags & __LWP_ASLWP) {
		__dynamic_tid = t->t_tid;
		__dynamic_lwpid = LWPID(t);
	}
}

static unsigned char	highbit_table[256] = {
	0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
	8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

/*
 * Find high bit set. returns bit number + 1, otherwise 0.
 */
int
_hibit(uint_t i)
{
	uint_t j;
	uint_t k;

	if (i == 0)
		return (0);
	if (j = (i >> 16)) {
		if (k = (j >> 8))
			return (highbit_table[k] + 24);
		else
			return (highbit_table[j] + 16);
	} else if (k = (i >> 8))
		return (highbit_table[k] + 8);
	return (highbit_table[i]);
}

/*
 * Find lowest one bit set.
 *      Returns bit number + 1 of lowest bit that is set, otherwise returns 0.
 * Low order bit is 0.
 */
int
lowbit(ulong i)
{
	int h = 1;

	if (i == 0)
		return (0);

	if (!(i & 0xffff)) {
		h += 16; i >>= 16;
	}
	if (!(i & 0xff)) {
		h += 8; i >>= 8;
	}
	if (!(i & 0xf)) {
		h += 4; i >>= 4;
	}
	if (!(i & 0x3)) {
		h += 2; i >>= 2;
	}
	if (!(i & 0x1)) {
		h += 1;
	}
	return (h);
}

int
_fsig(sigset_t *s)
{
	int i, sig;

	for (i = 0; i < 4; i++) {
		if (s->__sigbits[i])
			return (i*32 + lowbit(s->__sigbits[i]));
	}
	return (0);
}

/*
 * mask s2's signals out of the set of signals in s1 and
 * place the result in s3. Then, clear all the signals in s3
 * from s1 and place the result in s1.
 */
void
_sigunblock(sigset_t *s1, sigset_t *s2, sigset_t *s3)
{
	_sigmaskset(s1, s2, s3);
	if (!sigisempty(s3)) {
		s1->__sigbits[0] &= ~s3->__sigbits[0];
		s1->__sigbits[1] &= ~s3->__sigbits[1];
		s1->__sigbits[2] &= ~s3->__sigbits[2];
		s1->__sigbits[3] &= ~s3->__sigbits[3];
	}
}

/*
 * mask s2's signals out of the set of signals in s1 and
 * place the result in s3.
 */
void
_sigmaskset(sigset_t *s1, sigset_t *s2, sigset_t *s3)
{
	s3->__sigbits[0] = (s1->__sigbits[0] & ~s2->__sigbits[0]);
	s3->__sigbits[1] = (s1->__sigbits[1] & ~s2->__sigbits[1]);
	s3->__sigbits[2] = (s1->__sigbits[2] & ~s2->__sigbits[2]);
	s3->__sigbits[3] = (s1->__sigbits[3] & ~s2->__sigbits[3]);
}

int
_blocksent(sigset_t *sent, sigset_t *old, const sigset_t *new)
{
	int i;

	for (i = 0; i < 4; i++)
		if (~old->__sigbits[i] & new->__sigbits[i] & sent->__sigbits[i])
		    return (1);
	return (0);
}

int
_blocking(sigset_t *sent, sigset_t *old, const sigset_t *new, sigset_t *resend)
{
	int i;

	_sigemptyset(resend);
	for (i = 0; i < 4; i++)
		resend->__sigbits[i] = ~old->__sigbits[i] & new->__sigbits[i]
		    & sent->__sigbits[i];
	return (!sigisempty(resend));
}


void
_resume_ret(uthread_t *oldthread)
{
	uthread_t *t = curthread;

	ITRACE_0(UTR_FAC_TLIB_SWTCH, UTR_RESUME_END, "_resume end");
	TRACE_0(UTR_FAC_TRACE, UTR_THR_LWP_MAP, "dummy for thr_lwp mapping");

	/* preemption is disabled until _sigon() is called */
	ASSERT(t->t_nosig > 0);
	ASSERT(oldthread == NULL || (oldthread->t_state == TS_ZOMB ||
	    oldthread->t_flag & T_IDLETHREAD));
	ASSERT(t->t_idle->t_sig == 0);
	if (oldthread != NULL && oldthread->t_state == TS_ZOMB)
		_reapq_add(oldthread);
	_sched_lock_nosig();
	t->t_state = TS_ONPROC;
	if (PREEMPTED(t))
		t->t_preempt = 1;
	_sched_unlock_nosig();
	/*
	 * preemption is enabled by calling _sigon(). if thread was
	 * preempted, _sigon() will notice it and call _dopreempt().
	 */
	_sigon();
}


/*
 * Helper for _sigredirect()
 * Returns 0 on success and 1 failure
 */
static int
_sigredirect_thread(uthread_t *t, int sig, thread_t *savedtid, int firstpass,
			int ix)
{
	lwpid_t lwpid;

	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
	ASSERT(MUTEX_HELD(&_schedlock));

	if (!(t->t_flag & T_INTERNAL) &&
	    !_sigismember(&t->t_hold, sig) &&
	    t->t_state != TS_ZOMB &&
	    t->t_state != TS_STOPPED &&
	    t->t_preempt != 1 &&
	    t->t_stop == 0) {
		if ((t->t_state != TS_ONPROC) && firstpass) {
			if (!(*savedtid))
				*savedtid = t->t_tid;
			return (1);
		}
		_lwp_mutex_lock(&_pmasklock);
		if (_sigismember(&_pmask, sig)) {
			_sigdelset(&_pmask, sig);
			dbg_delset(&_tpmask, sig);
			_lwp_mutex_unlock(&_pmasklock);
			if (ISIGNORED(sig)) {
				/*
				* ignore this signal
				*/
				_lwp_sigredirect(0, sig);
				return (0);
			}
			if (__thr_sigredirect(t, ix, sig, &lwpid)) {
				/* __thr_sigredirect failed */
				_lwp_mutex_lock(&_pmasklock);
				dbg_sigaddset(&_tpmask, sig);
				_sigaddset(&_pmask, sig);
				_lwp_mutex_unlock(&_pmasklock);
				return (1);
			} else if (lwpid != NULL) {
				_lwp_sigredirect(lwpid, sig);
			}
			return (0);
		} else {
			dbg_delset(&_tpmask, sig);
			_lwp_mutex_unlock(&_pmasklock);
			return (0);
		}
	}
	return (1);
}
