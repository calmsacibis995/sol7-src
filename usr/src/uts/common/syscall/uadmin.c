/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uadmin.c	1.9	97/06/26 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/swap.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/var.h>
#include <sys/uadmin.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <vm/seg_kmem.h>
#include <sys/modctl.h>
#include <sys/debug.h>

/*
 * Administrivia system call.
 */

#define	BOOTSTRLEN	256

extern ksema_t fsflush_sema;
kmutex_t ualock;

int
uadmin(int cmd, int fcn, uintptr_t mdep)
{
	int error = 0;
	int rv = 0;
	int locked = 0;
	char *bootstr = NULL;
	char bootstrbuf[BOOTSTRLEN + 1];
	size_t len;

	/*
	 * Check cmd arg (fcn is system dependent & defaulted in mdboot())
	 * if wrong.
	 */
	switch (cmd) {
	case A_SWAPCTL:		/* swapctl checks permissions itself */
		if (get_udatamodel() == DATAMODEL_NATIVE)
			error = swapctl(fcn, (void *)mdep, &rv);
#if defined(_SYSCALL32_IMPL)
		else
			error = swapctl32(fcn, (void *)mdep, &rv);
#endif /* _SYSCALL32_IMPL */
		return (error ? (set_errno(error)) : rv);

	case A_SHUTDOWN:
	case A_REBOOT:
	case A_REMOUNT:
	case A_FREEZE:
		if (!suser(CRED()))
			return (set_errno(EPERM));
		break;

	default:
		return (set_errno(EINVAL));
	}

	switch (cmd) {
	case A_SHUTDOWN:
	case A_REBOOT:
		/*
		 * Copy in the boot string now.
		 * We will release our address space so we can't do it later.
		 */
		len = 0;
		if ((bootstr = (char *)mdep) != NULL &&
		    copyinstr(bootstr, bootstrbuf, BOOTSTRLEN, &len) == 0) {
			bootstrbuf[len] = 0;
			bootstr = bootstrbuf;
		} else {
			bootstr = NULL;
		}
		/* FALLTHROUGH */
	case A_REMOUNT:
		if (!mutex_tryenter(&ualock))
			return (0);
		locked = 1;
	}

	switch (cmd) {
	case A_SHUTDOWN:
	{
		register struct proc *p;
		struct vnode *exec_vp;

		/*
		 * Release (almost) all of our own resources.
		 */
		p = ttoproc(curthread);
		exitlwps(0);
		mutex_enter(&p->p_lock);
		p->p_flag |= SNOWAIT;
		sigfillset(&p->p_ignore);
		curthread->t_lwp->lwp_cursig = 0;
		if (p->p_exec) {
			exec_vp = p->p_exec;
			p->p_exec = NULLVP;
			mutex_exit(&p->p_lock);
			VN_RELE(exec_vp);
		} else {
			mutex_exit(&p->p_lock);
		}
		closeall(1);
		relvm();

		/*
		 * Kill all processes except kernel daemons and ourself.
		 * Make a first pass to stop all processes so they won't
		 * be trying to restart children as we kill them.
		 */
		mutex_enter(&pidlock);
		for (p = practive; p != NULL; p = p->p_next) {
			if (p->p_exec != NULLVP &&	/* kernel daemons */
			    p->p_as != &kas &&
			    p->p_stat != SZOMB) {
				mutex_enter(&p->p_lock);
				p->p_flag |= SNOWAIT;
				sigtoproc(p, NULL, SIGSTOP, 0);
				mutex_exit(&p->p_lock);
			}
		}
		p = practive;
		while (p != NULL) {
			if (p->p_exec != NULLVP &&	/* kernel daemons */
			    p->p_as != &kas &&
			    p->p_stat != SIDL &&
			    p->p_stat != SZOMB) {
				mutex_enter(&p->p_lock);
				if (sigismember(&p->p_sig, SIGKILL)) {
					mutex_exit(&p->p_lock);
					p = p->p_next;
				} else {
					sigtoproc(p, NULL, SIGKILL, 0);
					mutex_exit(&p->p_lock);
					(void) cv_timedwait(&p->p_srwchan_cv,
					    &pidlock, lbolt + hz);
					p = practive;
				}
			} else {
				p = p->p_next;
			}
		}
		mutex_exit(&pidlock);

		VN_RELE(u.u_cdir);
		if (u.u_rdir)
			VN_RELE(u.u_rdir);

		u.u_cdir = rootdir;
		u.u_rdir = NULL;

		/*
		 * Allow fsflush to finish running and then prevent it
		 * from ever running again so that vfs_unmountall() and
		 * vfs_syncall() can acquire the vfs locks they need.
		 */
		sema_p(&fsflush_sema);

		vfs_unmountall();

		(void) VFS_MOUNTROOT(rootvfs, ROOT_UNMOUNT);

		vfs_syncall();

		/* FALLTHROUGH */
	}

	case A_REBOOT:
		mdboot(cmd, fcn, bootstr);
		/* no return expected */
		break;

	case A_REMOUNT:
		/* remount root file system */
		(void) VFS_MOUNTROOT(rootvfs, ROOT_REMOUNT);
		break;

	case A_FREEZE:
	{
		/* XXX: declare in some header file */
		extern int cpr(int);

		if (modload("misc", "cpr") == -1)
			return (set_errno(EINVAL));

		error = cpr(fcn);
		return (error ? (set_errno(error)) : 0);
	}

	default:
		error = EINVAL;
	}

	if (locked)
		mutex_exit(&ualock);

	return (error ? (set_errno(error)) : 0);
}
