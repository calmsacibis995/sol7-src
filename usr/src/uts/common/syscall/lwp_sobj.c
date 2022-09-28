/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lwp_sobj.c	1.33	98/02/01 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/prsystm.h>
#include <sys/kmem.h>
#include <sys/sobject.h>
#include <sys/fault.h>
#include <sys/procfs.h>
#include <sys/watchpoint.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/machlock.h>
#include <sys/debug.h>
#include <sys/synch.h>
#include <sys/synch32.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/schedctl.h>
#include <sys/sleepq.h>
#include <sys/tnf_probe.h>
#include <sys/lwpchan_impl.h>
#include <vm/as.h>

static kthread_t *lwpsobj_owner(caddr_t);
static void lwp_unsleep(kthread_id_t t);
static void lwp_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip);
static void lwp_mutex_cleanup(lwpchan_entry_t *ent, uint16_t lockflg);
static void lwp_release_all(quad *lwpchan);

/*
 * The sobj_ops vector exports a set of functions needed when a thread
 * is asleep on a synchronization object of this type.
 */
static sobj_ops_t lwp_sobj_ops = {
	SOBJ_USER, lwpsobj_owner, lwp_unsleep, lwp_change_pri
};

static sleepq_head_t	lwpsleepq[NSLEEPQ];
static kmutex_t		lwpchanlock[LWPCHAN_BUCKETS];

#define	LWPCHAN_LOCAL	USYNC_THREAD
#define	LWPCHAN_GLOBAL	USYNC_PROCESS
#define	LWPCHAN_ROBUST	USYNC_PROCESS_ROBUST

#define	LOCKADDR(lname)	((lwp_mutex_t *) \
		((caddr_t)(lname) - (caddr_t)&(((lwp_mutex_t *)0)->mutex_type)))
/* is this a robust lock? */
#define	ROBUSTLOCK(type)	((type) & USYNC_PROCESS_ROBUST)

static sleepq_head_t *
lwpsqhash(quad *lwpchan)
{
	return (&lwpsleepq[SQHASHINDEX(lwpchan->val[1])]);
}

/*
 * lock an LWPCHAN. this will make an operation for this LWPCHAN
 * atomic.
 */
static void
lwpchan_lock(quad *lwpchan, int type)
{
	kmutex_t *lp;
	int i;

	ASSERT(curproc->p_lcp != NULL);

	i = LWPCHAN_HASH(lwpchan->val[1]);
	if (type == LWPCHAN_LOCAL)
		lp = &curproc->p_lcp->lwpchan_cache[i].lwpchan_lock;
	else
		lp = &lwpchanlock[i];
	mutex_enter(lp);
}

/*
 * unlock an LWPCHAN.
 */
static void
lwpchan_unlock(quad *lwpchan, int type)
{
	kmutex_t *lp;
	int i;

	ASSERT(curproc->p_lcp != NULL);

	i = LWPCHAN_HASH(lwpchan->val[1]);
	if (type == LWPCHAN_LOCAL)
		lp = &curproc->p_lcp->lwpchan_cache[i].lwpchan_lock;
	else
		lp = &lwpchanlock[i];
	mutex_exit(lp);
}

/*
 * delete mappings from the LWPCHAN cache for pages that are being
 * unmapped by as_unmap(). given a range of addresses, "start" to "end",
 * all mappings within this range will be deleted from the cache.
 */
void
lwpchan_delete_mapping(lwpchan_data_t *lcp, caddr_t start, caddr_t end)
{
	lwpchan_entry_t *ent, **prev;
	caddr_t a;
	int i;

	ASSERT(lcp != NULL);

	for (i = 0; i < LWPCHAN_BUCKETS; i++) {
		mutex_enter(&lcp->lwpchan_cache[i].lwpchan_lock);
		prev = &(lcp->lwpchan_cache[i].lwpchan_chain);
		/* check entire chain */
		while ((ent = *prev) != NULL) {
			a = ent->lwpchan_addr;
			if ((((caddr_t)LOCKADDR(a) +
				sizeof (lwp_mutex_t) - 1) >= start) &&
				a < end) {
				*prev = ent->lwpchan_next;
				lwp_mutex_cleanup(ent, LOCK_UNMAPPED);
				kmem_free(ent, sizeof (*ent));
			} else
				prev = &ent->lwpchan_next;
		}
		mutex_exit(&lcp->lwpchan_cache[i].lwpchan_lock);
	}
}

/*
 * allocate a per-process LWPCHAN cache. if memory cann't be allocated,
 * cache remains disabled.
 */
static void
lwpchan_alloc_cache(void)
{
	proc_t *p = curproc;
	lwpchan_data_t *p_lcp;

	if (p->p_lcp == NULL) {
		p_lcp = kmem_zalloc(sizeof (lwpchan_data_t), KM_SLEEP);
		/*
		 * typically, the lwp cache is allocated by a single thread,
		 * however there is nothing preventing two or more threads from
		 * doing this. when this happens, only one thread will be
		 * allowed to create the cache.
		 */
		mutex_enter(&p->p_lock);
		if (p->p_lcp == NULL) {
			p->p_lcp = p_lcp;
			mutex_exit(&p->p_lock);
			return;
		}
		mutex_exit(&p->p_lock);
		kmem_free(p_lcp, sizeof (lwpchan_data_t));
	}
}

/*
 * deallocate LWPCHAN cache, and any dynamically allocated mappings.
 * called when the process exits, all lwps except one have exitted.
 */
void
lwpchan_destroy_cache(void)
{
	lwpchan_entry_t *ent, *next;
	proc_t *p = curproc;
	int i;

	if (p->p_lcp != NULL) {
		for (i = 0; i < LWPCHAN_BUCKETS; i++) {
			ent = p->p_lcp->lwpchan_cache[i].lwpchan_chain;
			while (ent != NULL) {
				next = ent->lwpchan_next;
				if (ent->lwpchan_type == LWPCHAN_ROBUST)
					lwp_mutex_cleanup(ent, LOCK_OWNERDEAD);
				kmem_free(ent, sizeof (*ent));
				ent = next;
			}
		}
		kmem_free(p->p_lcp, sizeof (*p->p_lcp));
		p->p_lcp = NULL;
	}
}

/*
 * An LWPCHAN cache is allocated for the calling process the first time
 * the cache is referenced. This function returns 1 when the mapping
 * was in the cache and a zero when it is not.
 * caller of has the bucket lock.
 */
static int
lwpchan_cache_mapping(caddr_t addr, quad *lwpchan,
			lwpchan_hashbucket_t *hashbucket)
{
	lwpchan_entry_t *ent;
	int cachehit = 0;

	/*
	 * check cache for mapping.
	 */
	ent = hashbucket->lwpchan_chain;
	while (ent != NULL) {
		if (ent->lwpchan_addr == addr) {
			*lwpchan = *(quad *)&ent->lwpchan_lwpchan;
			cachehit = 1;
			break;
		}
		ent = ent->lwpchan_next;
	}
	return (cachehit);
}

/*
 * Return the cached mapping if cached, otherwise insert a virtual address to
 * LWPCHAN mapping into the cache. if the bucket for this mapping is not unique,
 * allocate an entry, and add it to the hash chain.
 */
static int
lwpchan_get_mapping(struct as *as, caddr_t addr, quad *lwpchan, int type)
{
	lwpchan_hashbucket_t *hashbucket;
	lwpchan_entry_t *ent, *new_ent;
	memid_t	memid;

	hashbucket = &(curproc->p_lcp->lwpchan_cache[LWPCHAN_HASH(addr)]);
	mutex_enter(&hashbucket->lwpchan_lock);
	if (lwpchan_cache_mapping(addr, lwpchan, hashbucket)) {
		mutex_exit(&hashbucket->lwpchan_lock);
		return (1);
	}
	mutex_exit(&hashbucket->lwpchan_lock);
	if (!as_getmemid(as, addr, &memid)) {
		/*
		 * we truncate ulonglong to long until lwpchan
		 * implementation is fixed
		 */
		lwpchan->val[0] = (long)memid.val[0];
		lwpchan->val[1] = (long)memid.val[1];
	} else {
		return (0);
	}
	new_ent = kmem_alloc(sizeof (lwpchan_entry_t), KM_SLEEP);
	mutex_enter(&hashbucket->lwpchan_lock);
	if (lwpchan_cache_mapping(addr, lwpchan, hashbucket)) {
		mutex_exit(&hashbucket->lwpchan_lock);
		kmem_free(new_ent, sizeof (*new_ent));
		return (1);
	}
	new_ent->lwpchan_addr = addr;
	new_ent->lwpchan_type = type;
	*(quad *)&new_ent->lwpchan_lwpchan = *lwpchan;
	ent = hashbucket->lwpchan_chain;
	if (ent == NULL) {
		hashbucket->lwpchan_chain = new_ent;
		new_ent->lwpchan_next = NULL;
	} else {
		new_ent->lwpchan_next = ent->lwpchan_next;
		ent->lwpchan_next = new_ent;
	}
	mutex_exit(&hashbucket->lwpchan_lock);
	return (1);
}

/*
 * Return a unique pair of identifiers (usually vnode/offset) that corresponds
 * to 'addr'.
 */
static int
get_lwpchan(struct as *as, caddr_t addr, int type, quad *lwpchan)
{
	/*
	 * initialize LWPCHAN cache.
	 */
	if (curproc->p_lcp == NULL)
		lwpchan_alloc_cache();

	/*
	 * if LWP synch object was defined to be local to this
	 * process, it's type field is set to zero. The first
	 * word of the lwpchan is curproc and the second word
	 * is the synch object's virtual address.
	 */
	if (type == LWPCHAN_LOCAL) {
		lwpchan->val[0] = (long)curproc;
		lwpchan->val[1] = (long)addr;
		return (1);
	}
	/* check LWPCHAN cache for mapping */
	return (lwpchan_get_mapping(as, addr, lwpchan, type));
}

static void
lwp_block(quad *lwpchan)
{
	klwp_t *lwp = ttolwp(curthread);
	sleepq_head_t *sqh;

	/*
	 * Put the lwp in an orderly state for debugging,
	 * just as though it stopped on a /proc request.
	 */
	prstop(lwp, PR_REQUESTED, 0);

	thread_lock(curthread);
	curthread->t_flag |= T_WAKEABLE;
	LWPCHAN(curthread)->val[0] = lwpchan->val[0];
	LWPCHAN(curthread)->val[1] = lwpchan->val[1];
	curthread->t_sobj_ops = &lwp_sobj_ops;
	sqh = lwpsqhash(lwpchan);
	disp_lock_enter_high(&sqh->sq_lock);
	THREAD_SLEEP(curthread, &sqh->sq_lock);
	sleepq_insert(&sqh->sq_queue, curthread);
	thread_unlock(curthread);
	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	lwp->lwp_ru.nvcsw++;
	if (curthread->t_proc_flag & TP_MSACCT)
		(void) new_mstate(curthread, LMS_SLEEP);
}


/*
 * A lwp blocks when the mutex is set.
 */
int
lwp_mutex_lock(lwp_mutex_t *lp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	klwp_t *lwp = ttolwp(t);
	int error = 0;
	u_char waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	quad lwpchan;
	register sleepq_head_t *sqh;
	static int iswanted();
	int scblock;
	uint16_t flag;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);
	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword8_noerr(&lp->mutex_waiters, &waiters);
	suword8_noerr(&lp->mutex_waiters, 1);
	if (ROBUSTLOCK(type)) {
		fuword16_noerr(&lp->mutex_flag, &flag);
		if (flag & LOCK_NOTRECOVERABLE) {
			lwpchan_unlock(&lwpchan, type);
			error = set_errno(ENOTRECOVERABLE);
			goto out;
		}
	}
	while (!ulock_try(&lp->mutex_lockw)) {
		if (mapped) {
			pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
			mapped = 0;
		}
		if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
			(void) schedctl_block(NULL);
		lwp_block(&lwpchan);
		/*
		 * Nothing should happen to cause the LWP to go
		 * to sleep again until after it returns from
		 * swtch().
		 */
		locked = 0;
		lwpchan_unlock(&lwpchan, type);
		if (ISSIG(t, JUSTLOOKING) || ISHOLD(p))
			setrun(t);
		swtch();
		t->t_flag &= ~T_WAKEABLE;
		if (scblock)
			schedctl_unblock();
		setallwatch();
		if (ISSIG(t, FORREAL) || lwp->lwp_sysabort || ISHOLD(p)) {
			error = set_errno(EINTR);
			lwp->lwp_asleep = 0;
			lwp->lwp_sysabort = 0;
			if (p->p_warea)
				mapped = pr_mappage((caddr_t)lp,
					sizeof (*lp), S_WRITE, 1);
			/*
			 * Need to re-compute waiters bit. The waiters field in
			 * the lock is not reliable. Either of two things
			 * could have occurred: no lwp may have called
			 * lwp_release() for me but I have woken up due to a
			 * signal. In this case, the waiter bit is incorrect
			 * since it is still set to 1, set above.
			 * OR an lwp_release() did occur for some other lwp
			 * on the same lwpchan. In this case, the waiter bit is
			 * correct. But which event occurred, one can't tell.
			 * So, recompute.
			 */
			lwpchan_lock(&lwpchan, type);
			locked = 1;
			sqh = lwpsqhash(&lwpchan);
			disp_lock_enter(&sqh->sq_lock);
			waiters = iswanted(sqh->sq_queue.sq_first, &lwpchan);
			disp_lock_exit(&sqh->sq_lock);
			break;
		}
		lwp->lwp_asleep = 0;
		if (p->p_warea)
			mapped = pr_mappage((caddr_t)lp, sizeof (*lp),
				S_WRITE, 1);
		lwpchan_lock(&lwpchan, type);
		locked = 1;
		fuword8_noerr(&lp->mutex_waiters, &waiters);
		suword8_noerr(&lp->mutex_waiters, 1);
		if (ROBUSTLOCK(type)) {
			fuword16_noerr(&lp->mutex_flag, &flag);
			if (flag & LOCK_NOTRECOVERABLE) {
				error = set_errno(ENOTRECOVERABLE);
				break;
			}
		}
	}
	if (!error && ROBUSTLOCK(type)) {
		suword32_noerr((uint32_t *)&(lp->mutex_ownerpid),
		    p->p_pidp->pid_id);
		fuword16_noerr(&lp->mutex_flag, &flag);
		if (flag & LOCK_OWNERDEAD) {
			error = set_errno(EOWNERDEAD);
		} else if (flag & LOCK_UNMAPPED) {
			error = set_errno(ELOCKUNMAPPED);
		}
	}
	suword8_noerr(&lp->mutex_waiters, waiters);
	locked = 0;
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

static int
iswanted(kthread_t *t, quad *lwpchan)
{
	/*
	 * The caller holds the dispatcher lock on the sleep queue.
	 */
	while (t != NULL) {
		if (LWPCHAN(t)->val[0] == lwpchan->val[0] &&
		    LWPCHAN(t)->val[1] == lwpchan->val[1]) {
			return (1);
		}
		t = t->t_link;
	}
	return (0);
}

static int
lwp_release(quad *lwpchan, u_char *waiters, int sync_type)
{
	register sleepq_head_t *sqh;
	register kthread_t *tp;
	register kthread_t **tpp;

	sqh = lwpsqhash(lwpchan);
	disp_lock_enter(&sqh->sq_lock);		/* lock the sleep queue */
	tpp = &sqh->sq_queue.sq_first;
	while ((tp = *tpp) != NULL) {
		if (LWPCHAN(tp)->val[0] == lwpchan->val[0] &&
		    LWPCHAN(tp)->val[1] == lwpchan->val[1]) {
			/*
			 * The following is typically false. It could be true
			 * only if lwp_release() is called from
			 * lwp_mutex_wakeup() after reading the waiters field
			 * from memory in which the lwp lock used to be, but has
			 * since been re-used to hold a lwp cv or lwp semaphore.
			 * The thread "tp" found to match the lwp lock's wchan
			 * is actually sleeping for the cv or semaphore which
			 * now has the same wchan. In this case, lwp_release()
			 * should return failure.
			 */
			if (sync_type != (tp->t_flag & T_WAITCVSEM)) {
				ASSERT(sync_type == 0);
				/*
				 * assert that this can happen only for mutexes
				 * i.e. sync_type == 0, for correctly written
				 * user programs.
				 */
				disp_lock_exit(&sqh->sq_lock);
				return (0);
			}
			*tpp = tp->t_link;
			*waiters = iswanted(tp->t_link, lwpchan);
			tp->t_wchan0 = 0;
			tp->t_wchan = NULL;
			tp->t_link = 0;
			tp->t_sobj_ops = NULL;
			THREAD_TRANSITION(tp);	/* drops sleepq lock */
			CL_WAKEUP(tp);
			thread_unlock(tp);	/* drop run queue lock */
			return (1);
		}
		tpp = &tp->t_link;
	}
	*waiters = 0;
	disp_lock_exit(&sqh->sq_lock);
	return (0);
}

static void
lwp_release_all(quad *lwpchan)
{
	register sleepq_head_t	*sqh;
	register kthread_id_t tp;
	register kthread_id_t *tpp;

	sqh = lwpsqhash(lwpchan);
	disp_lock_enter(&sqh->sq_lock);		/* lock sleep q queue */
	tpp = &sqh->sq_queue.sq_first;
	while ((tp = *tpp) != NULL) {
		if (LWPCHAN(tp)->val[0] == lwpchan->val[0] &&
		    LWPCHAN(tp)->val[1] == lwpchan->val[1]) {
			*tpp = tp->t_link;
			tp->t_wchan0 = 0;
			tp->t_wchan = NULL;
			tp->t_link = 0;
			tp->t_sobj_ops = NULL;
			CL_WAKEUP(tp);
			thread_unlock_high(tp);	/* release run queue lock */
		} else {
			tpp = &tp->t_link;
		}
	}
	disp_lock_exit(&sqh->sq_lock);		/* drop sleep q lock */
}

/*
 * unblock a lwp that is trying to acquire this mutex. the blocked
 * lwp resumes and retries to acquire the lock.
 */
int
lwp_mutex_wakeup(lwp_mutex_t *lp)
{
	proc_t *p = ttoproc(curthread);
	quad lwpchan;
	u_char waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	label_t ljb;
	int error = 0;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE, and type is USYNC_PROCESS
	 */
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	/*
	 * Always wake up an lwp (if any) waiting on lwpchan. The woken lwp will
	 * re-try the lock in _lwp_mutex_lock(). The call to lwp_release() may
	 * fail.  If it fails, do not write into the waiter bit.
	 * The call to lwp_release() might fail due to one of three reasons:
	 *
	 * 	1. due to the thread which set the waiter bit not actually
	 *	   sleeping since it got the lock on the re-try. The waiter
	 *	   bit will then be correctly updated by that thread. This
	 *	   window may be closed by reading the wait bit again here
	 *	   and not calling lwp_release() at all if it is zero.
	 *	2. the thread which set the waiter bit and went to sleep
	 *	   was woken up by a signal. This time, the waiter recomputes
	 *	   the wait bit in the return with EINTR code.
	 *	3. the waiter bit read by lwp_mutex_wakeup() was in
	 *	   memory that has been re-used after the lock was dropped.
	 *	   In this case, writing into the waiter bit would cause data
	 *	   corruption.
	 */
	if (lwp_release(&lwpchan, &waiters, 0) == 1) {
		suword8_noerr(&lp->mutex_waiters, waiters);
	}
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * lwp_cond_wait() has three arguments, a pointer to a condition variable,
 * a pointer to a mutex, and a pointer to a timeval for a timed wait.
 * The kernel puts the LWP to sleep on a unique 64 bit int called a
 * LWPCHAN.  The LWPCHAN is the concatenation of a vnode and a offset
 * which represents a physical memory address.  In this case, the
 * LWPCHAN used is the physical address of the condition variable.
 */
int
lwp_cond_wait(lwp_cond_t *cv, lwp_mutex_t *mp, timestruc_t *tsp)
{
	timestruc_t ts;			/* timed wait value */
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	proc_t *p = ttoproc(t);
	quad cv_lwpchan, m_lwpchan;
	caddr_t timedwait;
	volatile int type = 0;
	uint8_t mtype;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	caddr_t mp_lname;		/* logical name of lwp_mutex_t */
	u_char waiters, wt;
	int error = 0;
	volatile timeout_id_t id = 0;	/* timeout's id */
	clock_t tim, runtime;
	volatile int locked = 0;
	volatile int m_locked = 0;
	volatile int cvmapped = 0;
	volatile int mpmapped = 0;
	label_t ljb;
	int scblock;
	volatile int no_lwpchan = 1;

	if ((caddr_t)cv >= p->p_as->a_userlimit ||
	    (caddr_t)mp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);

	if (on_fault(&ljb)) {
		if (no_lwpchan)
			goto out;
		if (m_locked) {
			m_locked = 0;
			lwpchan_unlock(&m_lwpchan, type);
		}
		if (locked) {
			locked = 0;
			lwpchan_unlock(&cv_lwpchan, type);
		}
		/*
		 * set up another on_fault() for a possible fault
		 * on the user lock accessed at "efault"
		 */
		if (on_fault(&ljb)) {
			if (m_locked) {
				m_locked = 0;
				lwpchan_unlock(&m_lwpchan, type);
			}
			goto out;
		}
		goto efault;
	}

	if (p->p_warea) {
		cvmapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
		mpmapped = pr_mappage((caddr_t)mp, sizeof (*mp), S_WRITE, 1);
	}

	/*
	 * Force Copy-on-write fault if lwp_cond_t and lwp_mutex_t
	 * objects are defined to be MAP_PRIVATE, and are USYNC_PROCESS
	 */
	fuword32_noerr(&cv->cond_type, (uint32_t *)&type);
	suword32_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	fuword8_noerr(&mp->mutex_type, (uint8_t *)&mtype);
	suword8_noerr(&mp->mutex_type, mtype);
	mp_lname = (caddr_t)&mp->mutex_type;

	/* convert user level condition variable, "cv", to a unique LWPCHAN. */
	if (!get_lwpchan(p->p_as, cv_lname, type, &cv_lwpchan)) {
		goto out;
	}
	/* convert user level mutex, "mp", to a unique LWPCHAN */
	if (!get_lwpchan(p->p_as, mp_lname, type, &m_lwpchan)) {
		goto out;
	}
	no_lwpchan = 0;
	if ((timedwait = (caddr_t)tsp) != NULL) {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(timedwait, &ts, sizeof (ts)))
				goto out;
		} else {
			timestruc32_t ts32;
			if (copyin(timedwait, &ts32, sizeof (ts32)))
				goto out;
			TIMESPEC32_TO_TIMESPEC(&ts, &ts32);
		}
		tim = TIMESTRUC_TO_TICK(&ts);
		runtime = tim + lbolt;
		id = timeout((void (*)(void *))setrun, t, tim);
	}
	/*
	 * lwpchan_lock ensures that the calling LWP is put to sleep atomically
	 * with respect to a possible wakeup which is a result of either
	 * an lwp_cond_signal() or an lwp_cond_broadcast().
	 *
	 * What's misleading, is that the LWP is put to sleep after the
	 * condition variable's mutex is released. this is OK as long as
	 * the release operation is also done while holding lwpchan_lock. the
	 * LWP is then put to sleep when the possibility of pagefaulting
	 * or sleeping is completely eliminated.
	 */
	lwpchan_lock(&cv_lwpchan, type);
	locked = 1;
	if (LWPCHAN_HASH(cv_lwpchan.val[1]) != LWPCHAN_HASH(m_lwpchan.val[1])) {
		lwpchan_lock(&m_lwpchan, type);
		m_locked = 1;
	}
	suword8_noerr(&cv->cond_waiters, 1);
	/*
	 * unlock the condition variable's mutex. (pagefaults are possible
	 * here.)
	 */
	ulock_clear(&mp->mutex_lockw);
	fuword8_noerr(&mp->mutex_waiters, &wt);
	if (wt != 0) {
		/*
		 * Given the locking of lwpchan_lock around the release of the
		 * mutex and checking for waiters, the following call to
		 * lwp_release() can fail ONLY if the lock acquirer is
		 * interrupted after setting the waiter bit, calling lwp_block()
		 * and releasing lwpchan_lock. In this case, it could get pulled
		 * off the lwp sleep q (via setrun()) before the following call
		 * to lwp_release() occurs. In this case, the lock requestor
		 * will update the waiter bit correctly by re-evaluating it.
		 */
		if (lwp_release(&m_lwpchan, &waiters, 0) > 0)
			suword8_noerr(&mp->mutex_waiters, waiters);
	}
	if (mpmapped) {
		pr_unmappage((caddr_t)mp, sizeof (*mp), S_WRITE, 1);
		mpmapped = 0;
	}
	if (cvmapped) {
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
		cvmapped = 0;
	}
	if (m_locked) {
		lwpchan_unlock(&m_lwpchan, type);
		m_locked = 0;
	}
	if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
		(void) schedctl_block(NULL);
	t->t_flag |= T_WAITCVSEM;
	lwp_block(&cv_lwpchan);
	/*
	 * Nothing should happen to cause the LWP to go to sleep
	 * until after it returns from swtch().
	 */
	lwpchan_unlock(&cv_lwpchan, type);
	locked = 0;
	no_fault();
	if (ISSIG(t, JUSTLOOKING) || ISHOLD(p) ||
	    (timedwait && runtime - lbolt <= 0)) {
		setrun(t);
	}
	swtch();
	t->t_flag &= ~(T_WAITCVSEM | T_WAKEABLE);
	if (timedwait) {
		tim = untimeout(id);
	}
	if (scblock)
		schedctl_unblock();
	setallwatch();
	if (ISSIG(t, FORREAL) || lwp->lwp_sysabort || ISHOLD(p))
		error = set_errno(EINTR);
	else if (tim == -1)
		error = set_errno(ETIME);
	lwp->lwp_asleep = 0;
	lwp->lwp_sysabort = 0;
	/* mutex is re-acquired by caller */
	return (error);

efault:
	/*
	 * make sure that the user level lock is dropped before
	 * returning to caller, since the caller always re-acquires it.
	 */
	lwpchan_lock(&m_lwpchan, type);
	m_locked = 1;
	ulock_clear(&mp->mutex_lockw);
	fuword8_noerr(&mp->mutex_waiters, &wt);
	if (wt != 0) {
		/*
		 * See comment above on lock clearing and lwp_release()
		 * success/failure.
		 */
		if (lwp_release(&m_lwpchan, &waiters, 0) > 0)
			suword8_noerr(&mp->mutex_waiters, waiters);
	}
	m_locked = 0;
	lwpchan_unlock(&m_lwpchan, type);
out:
	/* Cancel outstanding timeout */
	if (id != (timeout_id_t)0)
		tim = untimeout(id);
	no_fault();
	if (mpmapped)
		pr_unmappage((caddr_t)mp, sizeof (*mp), S_WRITE, 1);
	if (cvmapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (set_errno(EFAULT));
}

/*
 * wakeup one lwp that's blocked on this condition variable.
 */
int
lwp_cond_signal(lwp_cond_t *cv)
{
	proc_t *p = ttoproc(curthread);
	quad lwpchan;
	u_char waiters, wt;
	volatile int type = 0;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	int error = 0;

	if ((caddr_t)cv >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_cond_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword32_noerr(&cv->cond_type, (uint32_t *)&type);
	suword32_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	if (!get_lwpchan(curproc->p_as, cv_lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword8_noerr(&cv->cond_waiters, &wt);
	if (wt != 0) {
		/*
		 * The following call to lwp_release() might fail but it is
		 * OK to write into the waiters bit below, since the memory
		 * could not have been re-used or unmapped (for correctly
		 * written user programs) as in the case of lwp_mutex_wakeup().
		 * For an incorrect program, we should not care about data
		 * corruption since this is just one instance of other places
		 * where corruption can occur for such a program. Of course
		 * if the memory is unmapped, normal fault recovery occurs.
		 */
		(void) lwp_release(&lwpchan, &waiters, T_WAITCVSEM);
		suword8_noerr(&cv->cond_waiters, waiters);
	}
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (error);
}

/*
 * wakeup every lwp that's blocked on this condition variable.
 */
int
lwp_cond_broadcast(lwp_cond_t *cv)
{
	proc_t *p = ttoproc(curthread);
	quad lwpchan;
	volatile int type = 0;
	caddr_t cv_lname;		/* logical name of lwp_cond_t */
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	u_char wt;
	int error = 0;

	if ((caddr_t)cv >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_cond_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword32_noerr(&cv->cond_type, (uint32_t *)&type);
	suword32_noerr(&cv->cond_type, type);
	cv_lname = (caddr_t)&cv->cond_type;
	if (!get_lwpchan(curproc->p_as, cv_lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword8_noerr(&cv->cond_waiters, &wt);
	if (wt != 0) {
		lwp_release_all(&lwpchan);
		suword8_noerr(&cv->cond_waiters, 0);
	}
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)cv, sizeof (*cv), S_WRITE, 1);
	return (error);
}

int
lwp_sema_trywait(volatile lwp_sema_t *sp)
{
	register kthread_t *t = curthread;
	register proc_t *p = ttoproc(t);
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile int type = 0;
	int tmpcnt;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	quad lwpchan;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword32_noerr((void *)&sp->sema_type, (uint32_t *)&type);
	suword32_noerr((void *)&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(p->p_as, sp_lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	if (tmpcnt > 0) {
		suword32_noerr((void *)&sp->sema_count, --tmpcnt);
		error = 0;
	} else {
		error = set_errno(EBUSY);
	}
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}

int
lwp_sema_wait(volatile lwp_sema_t *sp)
{
	register kthread_t *t = curthread;
	register klwp_t *lwp = ttolwp(t);
	register proc_t *p = ttoproc(t);
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile int type = 0;
	int tmpcnt;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	quad lwpchan;
	int scblock;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword32_noerr((void *)&sp->sema_type, (uint32_t *)&type);
	suword32_noerr((void *)&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(p->p_as, sp_lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	while (tmpcnt == 0) {
		suword8_noerr((void *)&sp->sema_waiters, 1);
		if (mapped) {
			pr_unmappage((caddr_t)sp, sizeof (*sp),
				S_WRITE, 1);
			mapped = 0;
		}
		if ((scblock = schedctl_check(t, SC_BLOCK)) != 0)
			(void) schedctl_block(NULL);
		t->t_flag |= T_WAITCVSEM;
		lwp_block(&lwpchan);
		/*
		 * Nothing should happen to cause the LWP to
		 * sleep again until after it returns from
		 * swtch().
		 */
		lwpchan_unlock(&lwpchan, type);
		locked = 0;
		if (ISSIG(t, JUSTLOOKING) || ISHOLD(p))
			setrun(t);
		swtch();
		t->t_flag &= ~(T_WAITCVSEM | T_WAKEABLE);
		if (scblock)
			schedctl_unblock();
		setallwatch();
		if (ISSIG(t, FORREAL) || lwp->lwp_sysabort || ISHOLD(p)) {
			lwp->lwp_asleep = 0;
			lwp->lwp_sysabort = 0;
			no_fault();
			return (set_errno(EINTR));
		}
		lwp->lwp_asleep = 0;
		if (p->p_warea)
			mapped = pr_mappage((caddr_t)sp,
				sizeof (*sp), S_WRITE, 1);
		lwpchan_lock(&lwpchan, type);
		locked = 1;
		fuword32_noerr((void *)&sp->sema_count, (uint32_t *)&tmpcnt);
	}
	suword32_noerr((void *)&sp->sema_count, --tmpcnt);
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}

int
lwp_sema_post(lwp_sema_t *sp)
{
	proc_t *p = ttoproc(curthread);
	u_char waiters;
	label_t ljb;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile int type = 0;
	caddr_t sp_lname;		/* logical name of lwp_sema_t */
	int tmpcnt;
	quad lwpchan;
	u_char wt;
	int error = 0;

	if ((caddr_t)sp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_sema_t object is
	 * defined to be MAP_PRIVATE, and is USYNC_PROCESS.
	 */
	fuword32_noerr(&sp->sema_type, (uint32_t *)&type);
	suword32_noerr(&sp->sema_type, type);
	sp_lname = (caddr_t)&sp->sema_type;
	if (!get_lwpchan(curproc->p_as, sp_lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	/*
	 * sp->waiters is only a hint. lwp_release() does nothing
	 * if there is no one waiting. The value of waiters is
	 * then set to zero.
	 */
	fuword8_noerr(&sp->sema_waiters, &wt);
	if (wt != 0) {
		(void) lwp_release(&lwpchan, &waiters, T_WAITCVSEM);
		suword8_noerr(&sp->sema_waiters, waiters);
	}
	fuword32_noerr(&sp->sema_count, (uint32_t *)&tmpcnt);
	suword32_noerr(&sp->sema_count, ++tmpcnt);
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, sizeof (*sp), S_WRITE, 1);
	return (error);
}
/*
 * Return the owner of the user-level s-object.
 * Since we can't really do this, return NULL.
 */
/* ARGSUSED */
static kthread_t *
lwpsobj_owner(caddr_t sobj)
{
	return ((kthread_t *)NULL);
}

/*
 * Wake up a thread asleep on a user-level synchronization
 * object.
 */
static void
lwp_unsleep(kthread_id_t t)
{
	ASSERT(THREAD_LOCK_HELD(t));
	if (t->t_wchan0 != 0) {
		register sleepq_head_t  *sqh;

		sqh = lwpsqhash((quad *)(&t->t_wchan0));
		if (sleepq_unsleep(&sqh->sq_queue, t) != NULL) {
			disp_lock_exit_high(&sqh->sq_lock);
			CL_SETRUN(t);
			return;
		}
	}
	cmn_err(CE_PANIC, "lwp_unsleep: thread %p not on sleepq", (void *)t);
}

/*
 * Change the priority of a thread asleep on a user-level
 * synchronization object. To maintain proper priority order,
 * we:
 *	o dequeue the thread.
 *	o change its priority.
 *	o re-enqueue the thread.
 * Assumption: the thread is locked on entry.
 */
static void
lwp_change_pri(kthread_t *t, pri_t pri, pri_t *t_prip)
{
	ASSERT(THREAD_LOCK_HELD(t));
	if (t->t_wchan0 != 0) {
		sleepq_head_t   *sqh;

		sqh = lwpsqhash((quad *)&t->t_wchan0);
		(void) sleepq_dequeue(&sqh->sq_queue, t);
		*t_prip = pri;
		sleepq_insert(&sqh->sq_queue, t);
	} else
		panic("lwp_change_pri: %p not on a sleep queue", (void *)t);
}

/*
 * Clean up a locked a robust mutex
 */
static void
lwp_mutex_cleanup(lwpchan_entry_t *ent, uint16_t lockflg)
{
	uint16_t flag;
	u_char waiters;
	label_t ljb;
	pid_t owner_pid;
	lwp_mutex_t *lp;
	volatile int locked = 0;
	volatile int mapped = 0;
	proc_t *p = curproc;

	lp = LOCKADDR(ent->lwpchan_addr);
	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&ent->lwpchan_lwpchan, LWPCHAN_GLOBAL);
		goto out;
	}
	fuword32_noerr((pid_t *)&(lp->mutex_ownerpid), (uint32_t *)&owner_pid);
	if (!(ent->lwpchan_type & USYNC_PROCESS_ROBUST) ||
		(owner_pid != curproc->p_pidp->pid_id)) {
		goto out;
	}
	lwpchan_lock(&ent->lwpchan_lwpchan, LWPCHAN_GLOBAL);
	locked = 1;
	fuword16_noerr(&(lp->mutex_flag), &flag);
	if ((flag & (LOCK_OWNERDEAD | LOCK_UNMAPPED)) == 0) {
		flag |= lockflg;
		suword16_noerr(&(lp->mutex_flag), flag);
	}
	suword32_noerr(&(lp->mutex_ownerpid), NULL);
	ulock_clear(&lp->mutex_lockw);
	fuword8_noerr(&(lp->mutex_waiters), &waiters);
	if (waiters && lwp_release(&ent->lwpchan_lwpchan, &waiters, 0)) {
		suword8_noerr(&lp->mutex_waiters, waiters);
	}
	lwpchan_unlock(&ent->lwpchan_lwpchan, LWPCHAN_GLOBAL);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
}

/*
 * Register the mutex and initialize the mutex if it is not already
 */
int
lwp_mutex_init(lwp_mutex_t *lp, int type)
{
	register proc_t *p = curproc;
	int error = 0;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	uint16_t flag;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	quad lwpchan;


	if ((caddr_t)lp >= (caddr_t)USERLIMIT)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	mutex_enter(&p->p_lcp_mutexinitlock);
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan)) {
		mutex_exit(&p->p_lcp_mutexinitlock);
		error = set_errno(EFAULT);
		goto out;
	}
	mutex_exit(&p->p_lcp_mutexinitlock);
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword16_noerr(&(lp->mutex_flag), &flag);
	if (flag & LOCK_INITED) {
		if (flag & (LOCK_OWNERDEAD | LOCK_UNMAPPED)) {
			flag &= ~(LOCK_OWNERDEAD | LOCK_UNMAPPED);
			suword16_noerr(&(lp->mutex_flag), flag);
			locked = 0;
			lwpchan_unlock(&lwpchan, type);
			goto out;
		} else
			error = set_errno(EBUSY);
	} else {
		suword8_noerr(&(lp->mutex_waiters), NULL);
		suword8_noerr(&(lp->mutex_lockw), NULL);
		suword16_noerr(&(lp->mutex_flag), LOCK_INITED);
		suword32_noerr(&(lp->mutex_ownerpid), NULL);
	}
	locked = 0;
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * non-blocking lwp_mutex_lock, returns EBUSY if can get the lock
 */
int
lwp_mutex_trylock(lwp_mutex_t *lp)
{
	kthread_t *t = curthread;
	proc_t *p = ttoproc(t);
	int error = 0;
	volatile int locked = 0;
	volatile int mapped = 0;
	label_t ljb;
	volatile uint8_t type = 0;
	uint16_t flag;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	quad lwpchan;

	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (t->t_proc_flag & TP_MSACCT)
		(void) new_mstate(t, LMS_USER_LOCK);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE and it was initialized to
	 * USYNC_PROCESS.
	 */
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	if (ROBUSTLOCK(type)) {
		fuword16_noerr((uint16_t *)(&lp->mutex_flag), &flag);
		if (flag & LOCK_NOTRECOVERABLE) {
			lwpchan_unlock(&lwpchan, type);
			error =  set_errno(ENOTRECOVERABLE);
			goto out;
		}
	}
	if (!ulock_try(&lp->mutex_lockw)) {
		error = set_errno(EBUSY);
		locked = 0;
		lwpchan_unlock(&lwpchan, type);
		goto out;
	}
	if (ROBUSTLOCK(type)) {
		suword32_noerr((uint32_t *)&(lp->mutex_ownerpid),
		    p->p_pidp->pid_id);
		if (flag & LOCK_OWNERDEAD) {
			error = set_errno(EOWNERDEAD);
		} else if (flag & LOCK_UNMAPPED) {
			error = set_errno(ELOCKUNMAPPED);
		}
	}
	locked = 0;
	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}

/*
 * unlock the mutex and unblock lwps that is trying to acquire this mutex.
 * the blocked lwp resumes and retries to acquire the lock.
 */
int
lwp_mutex_unlock(lwp_mutex_t *lp)
{
	proc_t *p = ttoproc(curthread);
	quad lwpchan;
	u_char waiters;
	volatile int locked = 0;
	volatile int mapped = 0;
	volatile uint8_t type = 0;
	caddr_t lname;		/* logical name of mutex (vnode + offset) */
	label_t ljb;
	uint16_t flag;
	int error = 0;


	if ((caddr_t)lp >= p->p_as->a_userlimit)
		return (set_errno(EFAULT));

	if (p->p_warea)
		mapped = pr_mappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);

	if (on_fault(&ljb)) {
		if (locked)
			lwpchan_unlock(&lwpchan, type);
		error = set_errno(EFAULT);
		goto out;
	}
	/*
	 * Force Copy-on-write fault if lwp_mutex_t object is
	 * defined to be MAP_PRIVATE, and type is USYNC_PROCESS
	 */
	fuword8_noerr(&lp->mutex_type, (uint8_t *)&type);
	suword8_noerr(&lp->mutex_type, type);
	lname = (caddr_t)&lp->mutex_type;
	if (!get_lwpchan(curproc->p_as, lname, type, &lwpchan)) {
		error = set_errno(EFAULT);
		goto out;
	}
	lwpchan_lock(&lwpchan, type);
	locked = 1;
	if (ROBUSTLOCK(type)) {
		fuword16_noerr(&(lp->mutex_flag), &flag);
		if (flag & (LOCK_OWNERDEAD |flag & LOCK_UNMAPPED)) {
			flag &= ~(LOCK_OWNERDEAD | LOCK_UNMAPPED);
			flag |= LOCK_NOTRECOVERABLE;
			suword16_noerr(&(lp->mutex_flag), flag);
		}
		suword32_noerr(&(lp->mutex_ownerpid), NULL);
	}
	ulock_clear(&lp->mutex_lockw);
	/*
	 * Always wake up an lwp (if any) waiting on lwpchan. The woken lwp will
	 * re-try the lock in _lwp_mutex_lock(). The call to lwp_release() may
	 * fail.  If it fails, do not write into the waiter bit.
	 * The call to lwp_release() might fail due to one of three reasons:
	 *
	 * 	1. due to the thread which set the waiter bit not actually
	 *	   sleeping since it got the lock on the re-try. The waiter
	 *	   bit will then be correctly updated by that thread. This
	 *	   window may be closed by reading the wait bit again here
	 *	   and not calling lwp_release() at all if it is zero.
	 *	2. the thread which set the waiter bit and went to sleep
	 *	   was woken up by a signal. This time, the waiter recomputes
	 *	   the wait bit in the return with EINTR code.
	 *	3. the waiter bit read by lwp_mutex_wakeup() was in
	 *	   memory that has been re-used after the lock was dropped.
	 *	   In this case, writing into the waiter bit would cause data
	 *	   corruption.
	 */
	fuword8_noerr(&(lp->mutex_waiters), &waiters);
	if (waiters) {
		if (ROBUSTLOCK(type) && (flag & LOCK_NOTRECOVERABLE)) {
			lwp_release_all(&lwpchan);
			suword8_noerr(&lp->mutex_waiters, 0);
		} else {
			if (lwp_release(&lwpchan, &waiters, 0) == 1) {
				suword8_noerr(&lp->mutex_waiters, waiters);

			}
		}
	}

	lwpchan_unlock(&lwpchan, type);
out:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)lp, sizeof (*lp), S_WRITE, 1);
	return (error);
}
