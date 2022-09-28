/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)turnstile.c	1.43	98/02/15 SMI"

/*
 * Big Theory Statement for turnstiles.
 *
 * Turnstiles provide blocking and wakeup support, including priority
 * inheritance, for synchronization primitives (e.g. mutexes and rwlocks).
 * Typical usage is as follows:
 *
 * To block on lock 'lp' for read access in foo_enter():
 *
 *	ts = turnstile_lookup(lp);
 *	[ If the lock is still held, set the waiters bit
 *	turnstile_block(ts, TS_READER_Q, lp, &foo_sobj_ops);
 *
 * To wake threads waiting for write access to lock 'lp' in foo_exit():
 *
 *	ts = turnstile_lookup(lp);
 *	[ Either drop the lock (change owner to NULL) or perform a direct
 *	[ handoff (change owner to one of the threads we're about to wake).
 *	[ If we're going to wake the last waiter, clear the waiters bit.
 *	turnstile_wakeup(ts, TS_WRITER_Q, nwaiters, new_owner or NULL);
 *
 * turnstile_lookup() returns holding the turnstile hash chain lock for lp.
 * Both turnstile_block() and turnstile_wakeup() drop the turnstile lock.
 * To abort a turnstile operation, the client must call turnstile_exit().
 *
 * Requirements of the client:
 *
 * (1) The lock's waiters indicator may be manipulated *only* while
 *     holding the turnstile hash chain lock (i.e. under turnstile_lookup()).
 *
 * (2) Once the lock is marked as having waiters, the owner may be
 *     changed *only* while holding the turnstile hash chain lock.
 *
 * (3) The caller must never block on an unheld lock.
 *
 * Consequences of these assumptions include the following:
 *
 * (a) It is impossible for a lock to be unheld but have waiters.
 *
 * (b) The priority inheritance code can safely assume that an active
 *     turnstile's ts_inheritor never changes until the inheritor calls
 *     turnstile_pi_waive().
 *
 * These assumptions simplify the implementation of both turnstiles and
 * their clients.
 *
 * Background on priority inheritance:
 *
 * Priority inheritance allows a thread to "will" its dispatch priority
 * to all the threads blocking it, directly or indirectly.  This prevents
 * situations called priority inversions in which a high-priority thread
 * needs a lock held by a low-priority thread, which cannot run because
 * of medium-priority threads.  Without PI, the medium-priority threads
 * can starve out the high-priority thread indefinitely.  With PI, the
 * low-priority thread becomes high-priority until it releases whatever
 * synchronization object the real high-priority thread is waiting for.
 *
 * How turnstiles work:
 *
 * All active turnstiles reside in a global hash table, turnstile_table[].
 * The address of a synchronization object determines its hash index.
 * Each hash chain is protected by its own dispatcher lock, acquired
 * by turnstile_lookup().  This lock protects the hash chain linkage, the
 * contents of all turnstiles on the hash chain, and the waiters bits of
 * every synchronization object in the system that hashes to the same chain.
 * Giving the lock such broad scope simplifies the interactions between
 * the turnstile code and its clients considerably.  The blocking path
 * is rare enough that this has no impact on scalability.  (If it ever
 * does, it's almost surely a second-order effect -- the real problem
 * is that some synchronization object is *very* heavily contended.)
 *
 * Each thread has an attached turnstile in case it needs to block.
 * A thread cannot block on more than one lock at a time, so one
 * turnstile per thread is the most we ever need.  The first thread
 * to block on a lock donates its attached turnstile and adds it to
 * the appropriate hash chain in turnstile_table[].  This becomes the
 * "active turnstile" for the lock.  Each subsequent thread that blocks
 * on the same lock discovers that the lock already has an active
 * turnstile, so it stashes its own turnstile on the active turnstile's
 * freelist.  As threads wake up, the process is reversed.
 *
 * turnstile_block() puts the current thread to sleep on the active
 * turnstile for the desired lock, walks the blocking chain to apply
 * priority inheritance to everyone in its way, and yields the CPU.
 *
 * turnstile_wakeup() waives any priority the owner may have inherited
 * and wakes the specified number of waiting threads.  If the caller is
 * doing direct handoff of ownership (rather than just dropping the lock),
 * the new owner automatically inherits priority from any existing waiters.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/turnstile.h>
#include <sys/t_lock.h>
#include <sys/disp.h>
#include <sys/sobject.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>

#define	TURNSTILE_HASH_SIZE	1		/* must be power of 2 */
#define	TURNSTILE_HASH_MASK	(TURNSTILE_HASH_SIZE - 1)
#define	TURNSTILE_SOBJ_HASH(sobj)	\
	((((int)sobj >> 2) + ((int)sobj >> 9)) & TURNSTILE_HASH_MASK)
#define	TURNSTILE_CHAIN(sobj)	turnstile_table[TURNSTILE_SOBJ_HASH(sobj)]

typedef struct turnstile_chain {
	turnstile_t	*tc_first;	/* first turnstile on hash chain */
	disp_lock_t	tc_lock;	/* lock for this hash chain */
} turnstile_chain_t;

turnstile_chain_t	turnstile_table[TURNSTILE_HASH_SIZE];

/*
 * Make 'inheritor' inherit priority from this turnstile.
 * Returns 1 if inheritance was necessary, 0 if not.
 */
static int
turnstile_pi_inherit(turnstile_t *ts, kthread_t *inheritor, pri_t epri)
{
	ASSERT(THREAD_LOCK_HELD(inheritor));
	ASSERT(DISP_LOCK_HELD(&TURNSTILE_CHAIN(ts->ts_sobj).tc_lock));

	if (epri <= inheritor->t_pri)
		return (0);

	if (ts->ts_inheritor == NULL) {
		ts->ts_inheritor = inheritor;
		ts->ts_epri = epri;
		ts->ts_prioinv = inheritor->t_prioinv;
		inheritor->t_prioinv = ts;
	} else {
		/*
		 * 'inheritor' is already inheriting from this turnstile,
		 * so just adjust its priority.
		 */
		ASSERT(ts->ts_inheritor == inheritor);
		if (ts->ts_epri < epri)
			ts->ts_epri = epri;
	}

	if (epri > DISP_PRIO(inheritor))
		thread_change_epri(inheritor, epri);

	return (1);
}

/*
 * Remove turnstile from inheritor's t_prioinv list, compute new inherited
 * priority, and change inheritor's effective priority if necessary.
 */
static void
turnstile_pi_waive(turnstile_t *ts)
{
	kthread_t *inheritor = ts->ts_inheritor;
	turnstile_t **tspp, *tsp;
	pri_t new_epri = 0;

	ASSERT(inheritor == curthread);

	thread_lock_high(inheritor);
	tspp = &inheritor->t_prioinv;
	while ((tsp = *tspp) != NULL) {
		if (tsp == ts)
			*tspp = tsp->ts_prioinv;
		else
			new_epri = MAX(new_epri, tsp->ts_epri);
		tspp = &tsp->ts_prioinv;
	}
	if (new_epri != DISP_PRIO(inheritor))
		thread_change_epri(inheritor, new_epri);
	ts->ts_inheritor = NULL;
	if (DISP_PRIO(inheritor) < DISP_MAXRUNPRI(inheritor))
		cpu_surrender(inheritor);
	thread_unlock_high(inheritor);
}

/*
 * Grab the lock protecting the hash chain for sobj
 * and return the active turnstile for sobj, if any.
 */
turnstile_t *
turnstile_lookup(void *sobj)
{
	turnstile_t *ts;
	turnstile_chain_t *tc = &TURNSTILE_CHAIN(sobj);

	disp_lock_enter(&tc->tc_lock);

	for (ts = tc->tc_first; ts != NULL; ts = ts->ts_next)
		if (ts->ts_sobj == sobj)
			break;

	return (ts);
}

/*
 * Drop the lock protecting the hash chain for sobj.
 */
void
turnstile_exit(void *sobj)
{
	disp_lock_exit(&TURNSTILE_CHAIN(sobj).tc_lock);
}

/*
 * Block the current thread on a synchronization object.
 */
void
turnstile_block(turnstile_t *ts, int qnum, void *sobj, sobj_ops_t *sobj_ops)
{
	kthread_t *owner;
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	turnstile_chain_t *tc = &TURNSTILE_CHAIN(sobj);
	int inverted;

	ASSERT(DISP_LOCK_HELD(&tc->tc_lock));

	thread_lock_high(t);

	if (ts == NULL) {
		/*
		 * This is the first thread to block on this sobj.
		 * Take its attached turnstile and add it to the hash chain.
		 */
		ts = t->t_ts;
		ts->ts_sobj = sobj;
		ts->ts_next = tc->tc_first;
		tc->tc_first = ts;
		ASSERT(ts->ts_waiters == 0);
	} else {
		/*
		 * Another thread has already donated its turnstile
		 * to block on this sobj, so ours isn't needed.
		 * Stash it on the active turnstile's freelist.
		 */
		turnstile_t *myts = t->t_ts;
		myts->ts_free = ts->ts_free;
		ts->ts_free = myts;
		t->t_ts = ts;
		ASSERT(ts->ts_sobj == sobj);
		ASSERT(ts->ts_waiters > 0);
	}

	/*
	 * Put the thread to sleep.
	 */
	ASSERT(t != CPU->cpu_idle_thread);
	ASSERT(CPU->cpu_on_intr == 0);
	ASSERT(t->t_wchan0 == 0 && t->t_wchan == NULL);
	ASSERT(t->t_state == TS_ONPROC);

	CL_SLEEP(t, 0);		/* assign kernel priority */
	THREAD_SLEEP(t, &tc->tc_lock);
	t->t_wchan = sobj;
	t->t_sobj_ops = sobj_ops;
	if (lwp != NULL) {
		lwp->lwp_ru.nvcsw++;
		if (t->t_proc_flag & TP_MSACCT)
			(void) new_mstate(t, LMS_SLEEP);
	}
	ts->ts_waiters++;
	sleepq_insert(&ts->ts_sleepq[qnum], t);

	/*
	 * Follow the blocking chain to its end, or until we run out of
	 * inversions, willing our priority to everyone who's in our way.
	 */
	while ((owner = SOBJ_OWNER(t->t_sobj_ops, t->t_wchan)) != NULL) {
		if (owner == curthread)
			panic("Deadlock detected: cycle in blocking chain");

		if (t->t_lockp != owner->t_lockp)
			thread_lock_high(owner);
		inverted = turnstile_pi_inherit(t->t_ts, owner, DISP_PRIO(t));
		if (t->t_lockp != owner->t_lockp)
			thread_unlock_high(t);
		t = owner;
		if (!inverted || t->t_sobj_ops == NULL)
			break;
	}
	thread_unlock_nopreempt(t);
	swtch();
}

/*
 * Wake threads that are blocked in a turnstile.
 */
void
turnstile_wakeup(turnstile_t *ts, int qnum, int nthreads, kthread_t *owner)
{
	turnstile_chain_t *tc = &TURNSTILE_CHAIN(ts->ts_sobj);
	turnstile_t *tsfree, **tspp;
	sleepq_t *sqp = &ts->ts_sleepq[qnum];

	ASSERT(DISP_LOCK_HELD(&tc->tc_lock));

	/*
	 * Waive any priority we may have inherited from this turnstile.
	 */
	if (ts->ts_inheritor != NULL)
		turnstile_pi_waive(ts);

	while (nthreads-- > 0) {
		kthread_t *t = sqp->sq_first;
		if ((tsfree = ts->ts_free) != NULL) {
			/*
			 * Take an unused turnstile from the active turnstile's
			 * freelist and attach it to the thread we're waking.
			 */
			ASSERT(ts->ts_waiters > 1);
			ASSERT(tsfree->ts_waiters == 0);
			t->t_ts = tsfree;
			ts->ts_free = tsfree->ts_free;
			tsfree->ts_free = NULL;
		} else {
			/*
			 * The active turnstile's freelist is empty, so this
			 * must be the last waiter.  Remove the turnstile
			 * from the hash chain and leave the now-inactive
			 * turnstile attached to the thread we're waking.
			 */
			ASSERT(ts->ts_waiters == 1);
			ASSERT(ts->ts_inheritor == NULL);
			ASSERT(nthreads == 0);
			tspp = &tc->tc_first;
			while (*tspp != ts)
				tspp = &(*tspp)->ts_next;
			*tspp = ts->ts_next;
			ASSERT(t->t_ts == ts);
		}
		ts->ts_waiters--;
		sqp->sq_first = t->t_link;
		t->t_link = NULL;
		t->t_sobj_ops = NULL;
		t->t_wchan = NULL;
		ASSERT(t->t_state == TS_SLEEP);
		CL_WAKEUP(t);
		/*
		 * If the caller did direct handoff of ownership,
		 * make the new owner inherit from this turnstile.
		 */
		if (t == owner) {
			kthread_t *wp = ts->ts_sleepq[TS_WRITER_Q].sq_first;
			kthread_t *rp = ts->ts_sleepq[TS_READER_Q].sq_first;
			pri_t wpri = wp ? DISP_PRIO(wp) : 0;
			pri_t rpri = rp ? DISP_PRIO(rp) : 0;
			(void) turnstile_pi_inherit(ts, t, MAX(wpri, rpri));
			owner = NULL;
		}
		thread_unlock_high(t);		/* drop run queue lock */
	}
	if (owner != NULL)
		panic("turnstile_wakeup: owner %p not woken", owner);
	disp_lock_exit(&tc->tc_lock);
}

/*
 * Change priority of a thread sleeping in a turnstile.
 */
void
turnstile_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip)
{
	turnstile_t *ts = t->t_ts;
	sleepq_t *sqp;

	if (sleepq_dequeue(sqp = &ts->ts_sleepq[TS_WRITER_Q], t) == NULL &&
	    sleepq_dequeue(sqp = &ts->ts_sleepq[TS_READER_Q], t) == NULL)
		panic("turnstile_change_pri: %p not in turnstile", t);
	*t_prip = pri;
	sleepq_insert(sqp, t);
}

/*
 * We don't allow spurious wakeups of threads blocked in turnstiles.
 * This is vital to the correctness of direct-handoff logic in some
 * synchronization primitives, and it also simplifies the PI logic.
 */
/* ARGSUSED */
void
turnstile_unsleep(kthread_t *t)
{
}
