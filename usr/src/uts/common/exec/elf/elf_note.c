/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)elf_notes.c	1.9	97/10/21 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/vmsystm.h>
#include <sys/debug.h>
#include <sys/procfs.h>
#ifdef sparc
#include <sys/elf_SPARC.h>
#endif /* sparc */
#ifdef i386
#include <sys/elf_386.h>
#endif /* i386 */
#include <sys/auxv.h>
#include <sys/exec.h>
#include <sys/prsystm.h>
#include <vm/as.h>
#include <vm/rm.h>
#include <sys/modctl.h>
#include <sys/systeminfo.h>
#include <sys/machelf.h>
#include "elf_impl.h"

extern	int	elfnote(vnode_t *, off_t *, int, int, caddr_t,
			rlim64_t, struct cred *);

void
setup_note_header(Phdr *v, proc_t *p)
{
	int nlwp = p->p_lwpcnt + p->p_zombcnt;
	size_t size;

	v[0].p_type = PT_NOTE;
	v[0].p_flags = PF_R;
	v[0].p_filesz = (sizeof (Note) * (4 + 2*nlwp))
	    + roundup(sizeof (psinfo_t), sizeof (Word))
	    + roundup(sizeof (pstatus_t), sizeof (Word))
	    + roundup(strlen(platform) + 1, sizeof (Word))
	    + roundup(__KERN_NAUXV_IMPL * sizeof (aux_entry_t),
	    sizeof (Word))
	    + nlwp * roundup(sizeof (lwpsinfo_t), sizeof (Word))
	    + nlwp * roundup(sizeof (lwpstatus_t), sizeof (Word));
	if ((size = prhasx()? prgetprxregsize() : 0) != 0)
		v[0].p_filesz += nlwp * sizeof (Note)
		    + nlwp * roundup(size, sizeof (Word));

#if defined(sparc) || defined(__sparc)
	/*
	 * Figure out the number and sizes of register windows.
	 */
	{
		kthread_t *t = p->p_tlist;
		do {
			if ((size = prnwindows(ttolwp(t))) != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				v[0].p_filesz += sizeof (Note) +
				    roundup(size, sizeof (Word));
			}
		} while ((t = t->t_forw) != p->p_tlist);
	}
#endif /* sparc */
}

int
write_elfnotes(proc_t *p, int sig,
	vnode_t *vp, off_t *offsetp, rlim64_t rlimit, cred_t *credp)
{
	union {
		psinfo_t	psinfo;
		pstatus_t	pstatus;
		lwpsinfo_t	lwpsinfo;
		lwpstatus_t	lwpstatus;
#if defined(sparc) || defined(__sparc)
		gwindows_t	gwindows;
#endif /* sparc */
		char		xregs[1];
		aux_entry_t	auxv[__KERN_NAUXV_IMPL];
	} *bigwad;
	size_t xregsize = prhasx()? prgetprxregsize() : 0;
	size_t bigsize = MAX(sizeof (*bigwad), xregsize);
	kthread_t *t;
	klwp_t *lwp;
	user_t *up;
	int i;
	int nlwp;
	int error;

	bigwad = kmem_alloc(bigsize, KM_SLEEP);

	/*
	 * The order of the elfnote entries should be same here
	 * and in the gcore(1) command.  Synchronization is
	 * needed between the kernel and gcore(1).
	 */

	mutex_enter(&p->p_lock);
	prgetpsinfo(p, &bigwad->psinfo);
	mutex_exit(&p->p_lock);
	error = elfnote(vp, offsetp, NT_PSINFO, sizeof (bigwad->psinfo),
	    (caddr_t)&bigwad->psinfo, rlimit, credp);
	if (error)
		goto done;

	mutex_enter(&p->p_lock);
	/*
	 * Restore current signal information
	 * in order to get a correct pstatus.
	 */
	lwp = ttolwp(curthread);
	ASSERT(lwp->lwp_cursig == 0);
	lwp->lwp_cursig = (u_char)sig;
	curthread->t_whystop = PR_FAULTED;	/* filthy */
	prgetstatus(p, &bigwad->pstatus);
	bigwad->pstatus.pr_lwp.pr_why = 0;
	curthread->t_whystop = 0;
	lwp->lwp_cursig = 0;
	mutex_exit(&p->p_lock);
	error = elfnote(vp, offsetp, NT_PSTATUS, sizeof (bigwad->pstatus),
	    (caddr_t)&bigwad->pstatus, rlimit, credp);
	if (error)
		goto done;

	error = elfnote(vp, offsetp, NT_PLATFORM, strlen(platform) + 1,
	    platform, rlimit, credp);
	if (error)
		goto done;

	up = PTOU(p);
	for (i = 0; i < __KERN_NAUXV_IMPL; i++) {
		bigwad->auxv[i].a_type = up->u_auxv[i].a_type;
		bigwad->auxv[i].a_un.a_val = up->u_auxv[i].a_un.a_val;
	}
	error = elfnote(vp, offsetp, NT_AUXV, sizeof (bigwad->auxv),
	    (caddr_t)bigwad->auxv, rlimit, credp);
	if (error)
		goto done;

	t = curthread;
	nlwp = p->p_lwpcnt;
	do {
		ASSERT(nlwp != 0);
		nlwp--;
		lwp = ttolwp(t);

		mutex_enter(&p->p_lock);
		prgetlwpsinfo(t, &bigwad->lwpsinfo);
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSINFO,
		    sizeof (bigwad->lwpsinfo), (caddr_t)&bigwad->lwpsinfo,
		    rlimit, credp);
		if (error)
			goto done;

		mutex_enter(&p->p_lock);
		if (t == curthread) {
			/*
			 * Restore current signal information
			 * in order to get a correct lwpstatus.
			 */
			lwp->lwp_cursig = (u_char)sig;
			t->t_whystop = PR_FAULTED;	/* filthy */
			prgetlwpstatus(t, &bigwad->lwpstatus);
			bigwad->lwpstatus.pr_why = 0;
			t->t_whystop = 0;
			lwp->lwp_cursig = 0;
		} else {
			prgetlwpstatus(t, &bigwad->lwpstatus);
		}
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSTATUS,
		    sizeof (bigwad->lwpstatus), (caddr_t)&bigwad->lwpstatus,
		    rlimit, credp);
		if (error)
			goto done;

#if defined(sparc) || defined(__sparc)
		/*
		 * Unspilled SPARC register windows.
		 */
		{
			size_t size = prnwindows(lwp);

			if (size != 0) {
				size = sizeof (gwindows_t) -
				    (SPARC_MAXREGWINDOW - size) *
				    sizeof (struct rwindow);
				prgetwindows(lwp, &bigwad->gwindows);
				error = elfnote(vp, offsetp, NT_GWINDOWS,
				    size, (caddr_t)&bigwad->gwindows,
				    rlimit, credp);
				if (error)
					goto done;
			}
		}
#endif /* sparc */

		if (xregsize) {
			prgetprxregs(lwp, bigwad->xregs);
			error = elfnote(vp, offsetp, NT_PRXREG,
			    xregsize, bigwad->xregs, rlimit, credp);
			if (error)
				goto done;
		}
	} while ((t = t->t_forw) != curthread);
	ASSERT(nlwp == 0);

	/* dump out zombied LWPs */
	nlwp = p->p_zombcnt;
	for (t = p->p_zomblist; nlwp-- > 0; t = t->t_forw) {
		lwp = ttolwp(t);

		mutex_enter(&p->p_lock);
		prgetlwpsinfo(t, &bigwad->lwpsinfo);
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSINFO,
		    sizeof (bigwad->lwpsinfo), (caddr_t)&bigwad->lwpsinfo,
		    rlimit, credp);
		if (error)
			goto done;

		mutex_enter(&p->p_lock);
		prgetlwpstatus(t, &bigwad->lwpstatus);
		mutex_exit(&p->p_lock);
		error = elfnote(vp, offsetp, NT_LWPSTATUS,
		    sizeof (bigwad->lwpstatus), (caddr_t)&bigwad->lwpstatus,
		    rlimit, credp);
		if (error)
			goto done;

		if (xregsize) {
			prgetprxregs(lwp, bigwad->xregs);
			error = elfnote(vp, offsetp, NT_PRXREG,
			    xregsize, bigwad->xregs, rlimit, credp);
			if (error)
				goto done;
		}
	}
	ASSERT(t == p->p_zomblist);

done:
	kmem_free(bigwad, bigsize);
	return (error);
}
