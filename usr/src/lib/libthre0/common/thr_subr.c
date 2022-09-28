/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)thr_subr.c	1.91	98/02/02 SMI"

#ifdef	TLS
#define	NOTHREAD
#endif

#include "libthread.h"
#include <sys/reg.h>
#include <dlfcn.h>

/*
 * Global variables
 */
#ifdef __STDC__
extern void *   _dlopen(const char *, int);
extern void *   _dlsym(void *, const char *);
#else
extern void *   _dlopen();
extern void *   _dlsym();
#endif

sigset_t _allmasked;	/* all except SIGCANCEL,SIGLWP,SIGKILL,SIGSTOP masked */
sigset_t _allunmasked;  /* all except SIGWAITING unmasked */
sigset_t _totalmasked;  /* all except SIGKILL,SIGSTOP masked */
sigset_t _cantreset;  	/* SA_RESETHAND has no impact on these signals */

int (*__sigtimedwait_trap)(const sigset_t *, siginfo_t *,
    const struct timespec *);
int (*__sigsuspend_trap)(const sigset_t *);
int (*__kill_trap)(pid_t, int);

int	_lpagesize;
struct	thread *_t0;	/* the initial thread */
mutex_t *_reaplockp;
sigset_t _null_sigset = {0, 0, 0, 0};

/*
 * Static variables
 */
static	sigset_t null_sigset_t = { 0, 0, 0, 0 };
static	struct itimerval null_itimerval = { 0, 0 };
static	int _t0once; /* XXX */
static	struct thread *_i0;	/* the initial lwp's idle thread */
static	char _i0stk[DAEMON_STACK]; /* i0's stack, later allocate dynamically */

/*
 * Static functions
 */
static	void _clean_thread(struct thread *t);

#define	STACK_ALIGNED(stk) (!((uintptr_t)(stk) & (STACK_ALIGN-1)))
#define	SA_DOWN(stk) (stk & ~(STACK_ALIGN-1))

#define	_REAP_HIGHMARK 100


/*
 * Idle threads are used to park idle LWPs that aren't bound to
 * a thread.
 */
uthread_t *
_idle_thread_create(int size)
{
	uthread_t *t;

	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_IDLE_CREATE_START,
	    "_idle_thread_create start");
	if (!_alloc_thread(NULL, size, &t, _lpagesize))
		return (NULL);
	t->t_startpc = _age;
	t->t_usropts = THR_BOUND|THR_DETACHED|THR_DAEMON;
	t->t_flag |= T_IDLETHREAD;
	t->t_flag |= T_INTERNAL;
	t->t_nosig = 1;
	t->t_tid = 0;
	t->t_pri = THREAD_MIN_PRIORITY;
	t->t_idle = t;
	_sched_lock();
	_totalthreads++;
	_sched_unlock();
	/*
	 * XXX: may be we should call masktotalsigs to mask SIGLWP/SIGCANCEL
	 * also. See comment in sigacthandler about avoiding a check
	 */
	t->t_hold = _null_sigset;
	t->t_state = TS_DISP;
	_lwp_sema_init(&t->t_park, NULL);
	t->t_park.sema_type = USYNC_THREAD;

	ITRACE_1(UTR_FAC_TLIB_MISC, UTR_IDLE_CREATE_END,
	    "_idle_thread_create end:tid 0x%x", t->t_tid);
	return (t);
}

/*
 * delete thread from _allthreads list and free it.
 * should be called with the thread's bucket lock held.
 * second arg is the tid's hash value (index into hash table).
 */
void
_thread_destroy(uthread_t *t, int ix)
{
	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
	_thread_delete(t, ix);
	_thread_free(t);
}

/*
 * delete thread from _allthreads list.
 */
void
_thread_delete(uthread_t *t, int ix) {
	ASSERT(MUTEX_HELD(&(_allthreads[ix].lock)));
	if (t->t_next == t) /* if last thread */
		_allthreads[ix].first = NULL;
	else {
		t->t_prev->t_next = t->t_next;
		t->t_next->t_prev = t->t_prev;
		if (_allthreads[ix].first == t)
			_allthreads[ix].first = t->t_next;
	}
}

void
_thread_free(uthread_t *t)
{
	if (t->t_flag & T_ALLOCSTK) {
		/*
		 * Call _free_stack with top of stack pointer..
		 */
		_free_stack(t->t_stk - t->t_stksize, t->t_stksize, 1,
				t->t_stkguardsize);
	}
}


/*
 * allocate thread local storage from a stack.
 */
int
_alloc_thread(caddr_t stk, size_t stksize, struct thread **tp, size_t guardsize)
{
	caddr_t tls;
	int newstack = 0;
	struct thread *t;

	guardsize = ROUNDUP_PAGESIZE(guardsize);
	if (stk == NULL) {
		newstack = 1;
		if (stksize == 0)
			stksize = DEFAULTSTACK;
		else {
			/* roundup to be a multiple of PAGESIZE */
			stksize = ROUNDUP_PAGESIZE(stksize);
		}
		if (_reapcnt > _REAP_HIGHMARK) {
			_reap_lock();
			while (_reapcnt > _REAP_HIGHMARK)
				_lwp_cond_wait(&_untilreaped, &_reaplock);
			_reap_unlock();
		}
		if (!_alloc_stack(stksize, &stk, guardsize))
			return (0);
		ITRACE_0(UTR_FAC_TLIB_MISC, UTR_THC_ALLOCSTK,
		    "_alloc_thread, after alloc_stk");
	}
	/*
	 * assign bottom of stack to stk.
	 * Round *down* to double word boundary.
	 */
	stk = (caddr_t)SA_DOWN((uintptr_t)stk + stksize);
	ASSERT(STACK_ALIGNED(stk));
#ifdef TLS
	tls = stk - SA(&_etls);
	t = (struct thread *)(tls + &_thread);
	t->t_tls = tls;
	t->t_sp = (long)(tls - SA(MINFRAME) - STACK_BIAS);
#else
	t = (struct thread *)((uintptr_t)stk - SA((sizeof (struct thread))));
	t->t_sp = (long)t - SA(MINFRAME) - STACK_BIAS;
	t->t_tls = NULL;
#endif TLS
	_clean_thread(t);
	ITRACE_0(UTR_FAC_TLIB_MISC, UTR_ALLOC_TLS_END,
	    "_alloc_thread end");
	/*
	 * Set t_stk and t_stksize so that thr_stksegment works for
	 * threads with user allocated stacks.
	 */
	t->t_stk = stk;
	t->t_stksize = stksize;
	t->t_stkguardsize = guardsize;

	if (newstack) {
		t->t_flag = T_ALLOCSTK;
		if ((stksize == DEFAULTSTACK) && (guardsize == _lpagesize))
			t->t_flag |= T_DEFAULTSTK;
	}
	*tp = t;
	return (1);
}

static void
_clean_thread(struct thread *t)
{
	t->t_idle = 0;
	t->t_link = 0;
	/* t_lock is not adaptive */
	t->t_lock.mutex_lockw = 0;
	t->t_next = t->t_prev = NULL;
	t->t_iforw = t->t_ibackw = NULL;
	t->t_clnup_hdr = NULL;
	t->t_usropts = 0;
	t->t_flag = 0;
	t->t_lwpdata = NULL;
	t->t_td_evbuf = NULL;

	/* zero t_state, t_nosig, t_stop, and t_preempt */
	*((int *)&(t->t_state)) = 0;
	/* zero t_schedlocked, t_pad, t_pending, and t_bpending */
	*((int *)&(t->t_schedlocked)) = 0;

	/* zero t_can stuff _state, _type, _pending and cancelable */
	*((int *)&(t->t_can_pending)) = 0;

	*(&(t->t_ssig)) = *(&(_null_sigset));
	*(&(t->t_bsig)) = *(&(_null_sigset));
	t->t_psig = _null_sigset;
	t->t_realitimer = null_itimerval;
	t->t_wchan = NULL;
	t->t_errno = 0;
	t->t_startpc = 0;
	/* PROBE_SUPPORT begin */
	t->t_tpdp = NULL;
	/* PROBE_SUPPORT end */
}



#ifdef i386
extern void __freegs(void);
void (*__freegsp)();
#endif
void (*_lwp_exitp)(void);
int (*_lwp_mutex_unlockp)();


/* PROBE_SUPPORT begin */
#pragma weak __tnf_probe_notify
/* PROBE_SUPPORT end */

#ifdef DEBUG
int dbg = 1;
#endif

/*
 * initialize the primordial thread. this is not a re-entrant
 * routine. It is only executed once and is used to turn the
 * primordial thread into a real thread.
 */
void
_t0init(void)
{
	caddr_t stk;
	caddr_t i0tls;
	caddr_t tls;
	uintptr_t diff;
	extern __lwp_mutex_unlock();
	extern void __lwp_exit();
	void * libc_hdl;

	sigset_t initmask;
#ifdef TLS
	extern int _etls;
#endif
	/* PROBE_SUPPORT begin */
	extern void __tnf_probe_notify(void);
	void  (*tnf_fptr)(void);
	/* PROBE_SUPPORT end */
	int i;
#if defined(ITRACE)|| defined(UTRACE)
	extern void trace_close();
#endif

	if (_t0once++)
		return;

	/* initialize an idle thread */
	stk = (caddr_t)SA_DOWN((uintptr_t)_i0stk + DAEMON_STACK);
	diff = _i0stk + DAEMON_STACK - stk;
#ifdef TLS
	i0tls = stk - SA((uintptr_t)&_etls);
	_i0 = (struct thread *)(i0tls + (uintptr_t)&_thread);
	_i0->t_sp = (uintptr_t)i0tls - SA(MINFRAME) - STACK_BIAS;
	_i0->t_tls = i0tls;
#else
	_i0 = (struct thread *)
	    SA_DOWN((uintptr_t)stk - sizeof (struct thread));
	_i0->t_sp = (uintptr_t)_i0 - SA(MINFRAME) - STACK_BIAS;
	_i0->t_tls = NULL;
#endif
	_clean_thread(_i0);
	_i0->t_startpc = _age;
	_i0->t_stk = stk;
	/*
	 * make sure t_stksize is adjusted for the rounding down due to SA_DOWN
	 * above, so that _free_stack (t_stk - t_stksize, ..) works correctly
	 */
	_i0->t_stksize = DAEMON_STACK - diff;
	_i0->t_pri = THREAD_MIN_PRIORITY;
	_i0->t_tid = 0;
	_i0->t_idle = _i0;
	_i0->t_lwpid = _lwp_self();
	_i0->t_nosig = 1;
	_i0->t_usropts = THR_BOUND|THR_DETACHED|THR_DAEMON;
	_i0->t_flag = T_IDLETHREAD;
	_i0->t_flag |= T_INTERNAL;
	_i0->t_idle = _i0;
	_i0->t_state = TS_ONPROC;
	maskallsigs(&_i0->t_hold);
	_lwp_sema_init(&_i0->t_park, 0);
	_i0->t_park.sema_type = USYNC_THREAD;
	_init_cpu(_i0);
#ifdef TLS
	tls = _sbrk(SA((uintptr_t)&(_etls)));
#else
	tls = _sbrk(sizeof (uthread_t) + STACK_ALIGN);
#endif

	_lpagesize = PAGESIZE;

	_sigemptyset(&_cantmask);
	_sigaddset(&_cantmask, SIGKILL);
	_sigaddset(&_cantmask, SIGSTOP);
	_lcantmask = _cantmask;
	_sigaddset(&_cantmask, SIGLWP);
	_sigaddset(&_cantmask, SIGCANCEL);

	_sigemptyset(&_cantreset);
	_sigaddset(&_cantreset, SIGILL);
	_sigaddset(&_cantreset, SIGPWR);
	_sigaddset(&_cantreset, SIGTRAP);

	maskallsigs(&_allmasked);
	masktotalsigs(&_totalmasked);

	/* initialize the primordial thread */
#ifndef	MAX_ALIGNMENT
#define	MAX_ALIGNMENT	8
#define	_MAXALIGN(x)	(((x) + MAX_ALIGNMENT-1) & ~(MAX_ALIGNMENT-1))
#endif

#ifdef TLS
	/* this is suspect */
	_t0 = (struct thread *)
		(_MAXALIGN((uintptr_t)tls + (uintptr_t)&_thread));
	_t0->t_tls = tls;
	_init_cpu(tls);
#else
	_t0 = (struct thread *)SA((uintptr_t)tls);
	_t0->t_tls = NULL;
	_init_cpu(_t0);
#endif
	_clean_thread(_t0);

	/*
	 * PROBE_SUPPORT begin
	 *
	 *  notify libtnf that primordial thread has been set up
	 *  CAUTION: must be after _init_cpu and _clean_thread and before
	 *		other threads are created.
	 */
	if ((tnf_fptr = __tnf_probe_notify) != 0) {
		(*tnf_fptr)();
	}
	/* PROBE_SUPPORT end */

	/*
	 * t_stk and t_stksize are never used for _t0.
	 * And t_sp will be set when _t0 switches.
	 */
	_t0->t_startpc = 0;
	_t0->t_state = TS_ONPROC;
	_t0->t_pri = THREAD_MIN_PRIORITY;
	_t0->t_tid = ++_lasttid;
	_t0->t_lwpid = _lwp_self();
	_t0->t_idle = _i0;
	if (!_lock_try(&(_t0->t_lock.mutex_lockw)))
		_panic("init failed: init thead's lock not available!");
	_totalthreads = _nthreads = _userthreads = 1;
	_lwp_sema_init(&_t0->t_park, NULL);
	_t0->t_park.sema_type = USYNC_THREAD;

	/*
	 * XXX:
	 * Check inheritance rule of inheriting signal mask from parent.
	 * It may be OK  to initialize first thread to have signals unblocked.
	 */
	__sigprocmask(SIG_SETMASK, NULL, &initmask);
	_t0->t_hold = initmask;
	__sigprocmask(SIG_SETMASK, &_null_sigset, NULL);
	/*
	 * Install SIG_DFL as disposition at user level for all signals.
	 * Initialize the ignoredefault set and also set up SIGLWP handling.
	 */
	_initsigs();

	_t0->t_next = _t0->t_prev = _t0;
	_allthreads[HASH_TID(_t0->t_tid)].first = _t0;
	_nlwps = 1;
	_t0->t_forw = _t0->t_backw = _t0;
	_onprocq = _t0;

	/*
	 * WARNING:  No thread creates above this point.
	 *
	 * No thread creates can occur until the rtld and libc interfaces
	 * have been set.
	 */
	_set_rtld_interface();
	_set_libc_interface();
	_pthread_atfork(_libc_prepare_atfork, _libc_parent_atfork,
			_libc_child_atfork);

	_sys_thread_create(_dynamiclwps, __LWP_ASLWP);

	_sc_init();

	/*
	 * See note in _lwp_terminate(). Pointers to the functions
	 * _lwp_mutex_unlock and _lwp_exit are stored in their respective
	 * *resolved* variables, _lwp_mutex_unlockp and _lwp_exitp. These are
	 * used in _lwp_terminate() to call into the respective functions after
	 * switching to the small stack. The same is true for _reaplock.
	 * Since these variables are already resolved, the dynamic linker does
	 * not get called in _lwp_terminate(). If this is not done, the
	 * calls to _lwp_exit, _lwp_mutex_unlock, etc. could invoke the dynamic
	 * linker in _lwp_terminate() after switching to the small stack,
	 * which would cause a stack overflow.
	 * Do the same if you add any new references to symbols in
	 * _lwp_terminate() after the switch to the small stack.
	 */

	/*
	 * Resolve the strong symbols (i.e. those with __ in front.)
	 * This is because, e.g. if you use _lwp_exit, and there is a call
	 * elsewhere in libthread to _lwp_exit (say, in _age()), the
	 * assignment of &_lwp_exit to _lwp_exitp will result in a
	 * pointer into the PLT entry and not directly to the function.
	 * So, the first time _lwp_exit is called via _lwp_exitp(), it
	 * will jump into the linker, which is not desirable.
	 */
	_lwp_exitp = (void (*)())(&__lwp_exit);
	_lwp_mutex_unlockp = &__lwp_mutex_unlock;
#ifdef i386
	__freegsp = &__freegs;
#endif
	_reaplockp = &_reaplock;

#if defined(ITRACE)|| defined(UTRACE)
	atexit(trace_close);
	enable_facility(UTR_FAC_TRACE);
	enable_facility(UTR_FAC_THREAD);
	enable_facility(UTR_FAC_THREAD_SYNC);
	trace_on();
#endif
	libc_hdl = _dlopen("libc.so.1", RTLD_LAZY);
	if (libc_hdl == NULL)
		_panic("init failed: dlopen libc failed");
	__sigtimedwait_trap = (int (*)())_dlsym(libc_hdl, "_libc_sigtimedwait");
	__sigsuspend_trap = (int (*)())_dlsym(libc_hdl, "_sigsuspend");
	__kill_trap = (int (*)())_dlsym(libc_hdl, "_kill");
	if (__sigtimedwait_trap == NULL || __sigsuspend_trap == NULL ||
	    __kill_trap == NULL)
		_panic("init failed: _dlsym for symbols in libc");
#ifdef DEBUG
	if (getenv("THREAD_NODEBUG") != NULL)
		dbg = 0;
#endif
	_reaper_create();
}

/*
 * destroy all threads except curthread
 * Used in fork1() wrapper.
 */
void
_thread_free_all()
{
	int ix;
	uthread_t *t, *next, *first;

	for (ix = 0; ix < ALLTHR_TBLSIZ; ix++) {
		_lock_clear_adaptive(&_allthreads[ix].lock);
		if ((first = t = _allthreads[ix].first) != NULL) {
			_allthreads[ix].first = NULL;
			do {
				next = t->t_next;
				if ((t != curthread) &&
				    (t->t_flag & T_ALLOCSTK)) {
					_free_stack(t->t_stk - t->t_stksize,
					    t->t_stksize, 0, t->t_stkguardsize);
				}
			} while ((t = next) != first);
		}

	}
}
