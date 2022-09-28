/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994,1997 by Sun Microsystems, Inc. */
/*	  All rights reserved.	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)lwp_create.c	1.15	97/10/03 SMI" /* from SVr4.0 1.78 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/syscall.h>
#include <sys/proc.h>
#include <sys/processor.h>
#include <sys/fault.h>
#include <sys/ucontext.h>
#include <sys/signal.h>
#include <sys/unistd.h>
#include <sys/procfs.h>
#include <sys/prsystm.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/klwp.h>

/*
 * A process can create a special lwp, the "aslwp", to take signals sent
 * asynchronously to this process. The "aslwp" (short for Asynchronous Signals'
 * lwp) is like a daemon lwp within this process and it is the first recipient
 * of any signal sent asynchronously to the containing process. The aslwp is
 * created via a new, reserved flag (__LWP_ASLWP) to _lwp_create(2). Currently
 * only an MT process, i.e. a process linked with -lthread, creates such an lwp.
 * At user-level, "aslwp" is usually in a "sigwait()", waiting for all signals.
 * The aslwp is set up by calling setup_aslwp() from syslwp_create().
 */
static void
setup_aslwp(kthread_t *t)
{
	proc_t *p = ttoproc(t);

	ASSERT(MUTEX_HELD(&p->p_lock));
	ASSERT((p->p_flag & ASLWP) == 0 && p->p_aslwptp == NULL);
	p->p_flag |= ASLWP;
	p->p_aslwptp = t;
	/*
	 * Since "aslwp"'s thread pointer has just been advertised above, it
	 * is impossible for it to have received any signals directed via
	 * sigtoproc(). They are all in p_sig and the sigqueue is all in
	 * p_sigqueue.
	 */
	ASSERT(sigisempty(&t->t_sig));
	ASSERT(t->t_sigqueue == NULL);
	t->t_sig = p->p_sig;
	sigemptyset(&p->p_sig);
	t->t_sigqueue = p->p_sigqueue;
	p->p_sigqueue = NULL;
	/*
	 * initialize binding state - might have been initialized
	 * inappropriately in "lwp_create()".
	 */
	t->t_bind_cpu = PBIND_NONE;
	t->t_bound_cpu = 0;
}

/*
 * Create a lwp.
 */
int
syslwp_create(ucontext_t *ucp, int flags, int *new_lwp)
{
	klwp_id_t lwp;
	proc_t *p = ttoproc(curthread);
	kthread_t *t;
	ucontext_t uc;
#ifdef _SYSCALL32_IMPL
	ucontext32_t uc32;
#endif /* _SYSCALL32_IMPL */
	k_sigset_t sigmask;
	int	tid;
	model_t model = get_udatamodel();

	/*
	 * lwp_create() is not supported for the /proc agent lwp.
	 */
	if (curthread == p->p_agenttp)
		return (set_errno(ENOTSUP));

	if (model == DATAMODEL_NATIVE) {
		if (copyin(ucp, &uc, sizeof (ucontext_t)))
			return (set_errno(EFAULT));
		sigutok(&uc.uc_sigmask, &sigmask);
	}
#ifdef _SYSCALL32_IMPL
	else {
		if (copyin(ucp, &uc32, sizeof (ucontext32_t)))
			return (set_errno(EFAULT));
		sigutok(&uc32.uc_sigmask, &sigmask);
		ucontext_32ton(&uc32, &uc, NULL, NULL);
	}
#endif /* _SYSCALL32_IMPL */

	(void) save_syscall_args();	/* save args for tracing first */
	lwp = lwp_create(lwp_rtt, NULL, NULL, curproc, TS_STOPPED,
		curthread->t_pri, sigmask, curthread->t_cid);
	if (lwp == NULL)
		return (set_errno(EAGAIN));

	lwp_load(lwp, uc.uc_mcontext.gregs);

	t = lwptot(lwp);
	/*
	 * copy new LWP's lwpid_t into the caller's specified buffer.
	 */
	if (new_lwp) {
		if (copyout((char *)&t->t_tid, (char *)new_lwp, sizeof (int))) {
			/*
			 * caller's buffer is not writable, return
			 * EFAULT, and terminate new LWP.
			 */
			mutex_enter(&p->p_lock);
			t->t_proc_flag |= TP_EXITLWP;
			t->t_sig_check = 1;
			t->t_sysnum = 0;
			lwp_continue(t);
			mutex_exit(&p->p_lock);
			return (set_errno(EFAULT));
		}
	}

	mutex_enter(&p->p_lock);
	/*
	 * Copy the syscall arguments to the new lwp's arg area
	 * for the benefit of debuggers.
	 */
	t->t_sysnum = SYS_lwp_create;
	lwp->lwp_ap = lwp->lwp_arg;
	lwp->lwp_arg[0] = (long)ucp;
	lwp->lwp_arg[1] = (long)flags;
	lwp->lwp_arg[2] = (long)new_lwp;
	lwp->lwp_argsaved = 1;

	/*
	 * If we are creating the aslwp, do some checks then set it up.
	 */
	if (flags & __LWP_ASLWP) {
		if (p->p_flag & ASLWP) {
			/*
			 * There is already an aslwp.
			 * Return EINVAL and terminate the new LWP.
			 */
			t->t_proc_flag |= TP_EXITLWP;
			t->t_sig_check = 1;
			t->t_sysnum = 0;
			lwp_continue(t);
			mutex_exit(&p->p_lock);
			return (set_errno(EINVAL));
		}
		setup_aslwp(t);
	}

	if (!(flags & LWP_DETACHED))
		t->t_proc_flag |= TP_TWAIT;

	tid = (int)t->t_tid;	/* for debugger */

	if (!(flags & LWP_SUSPENDED))
		lwp_continue(t);		/* start running */
	else {
		t->t_proc_flag |= TP_HOLDLWP;	/* create suspended */
		prstop(lwp, PR_SUSPENDED, SUSPEND_NORMAL);
	}

	mutex_exit(&p->p_lock);

	return (tid);
}

/*
 * Exit the calling lwp
 */
void
syslwp_exit()
{
	proc_t *p = ttoproc(curthread);

	mutex_enter(&p->p_lock);
	lwp_exit();
	/* NOTREACHED */
}
