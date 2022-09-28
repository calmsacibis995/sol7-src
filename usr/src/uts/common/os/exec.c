/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)exec.c 1.103     98/02/10 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/acct.h>
#include <sys/cpuvar.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/pathname.h>
#include <sys/vm.h>
#include <sys/vtrace.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/kmem.h>
#include <sys/prsystm.h>
#include <sys/modctl.h>
#include <sys/vmparam.h>
#include <sys/schedctl.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>

int nullmagic = 0;		/* null magic number */

static int execsetid(struct vnode *, struct vattr *, uid_t *, uid_t *);
static int hold_execsw(struct execsw *);

int auxv_hwcap = 0;	/* auxv AT_SUN_HWCAP value; determined on the fly */
int kauxv_hwcap = 0;	/* analogous kernel version of the same flag */

#ifdef i386
extern void ldt_free(proc_t *pp);
#endif /* i386 */


/*
 * exec() - wrapper around exece providing NULL environment pointer
 */
int
exec(const char *fname, const char **argp)
{
	return (exece(fname, argp, NULL));
}

/*
 * exece() - system call wrapper around exec_common()
 */
int
exece(const char *fname, const char **argp, const char **envp)
{
	int error;

	error = exec_common(fname, argp, envp);
	return (error ? (set_errno(error)) : 0);
}

int
exec_common(const char *fname, const char **argp, const char **envp)
{
	vnode_t *vp = NULL;
	proc_t *p = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	struct user *up = PTOU(p);
	long execsz;		/* temporary count of exec size */
	int i;
	int error = 0;
	char exec_file[MAXCOMLEN+1];
	struct pathname pn;
	struct pathname resolvepn;
	struct uarg args;
	struct execa ua;

	/*
	 * exec() is not supported for the /proc agent lwp
	 * or for the aslwp.  Return error.
	 */
	if (curthread == p->p_agenttp)
		return (ENOTSUP);
	if (curthread == p->p_aslwptp)
		return (EACCES);

	/*
	 * Look up path name and remember last component for later.
	 */
	if (error = pn_get((char *)fname, UIO_USERSPACE, &pn))
		return (error);
	pn_alloc(&resolvepn);
	if (error = lookuppn(&pn, &resolvepn, FOLLOW, NULLVPP, &vp)) {
		pn_free(&resolvepn);
		pn_free(&pn);
		return (error);
	}
	bzero(exec_file, MAXCOMLEN+1);
	(void) strncpy(exec_file, pn.pn_path, MAXCOMLEN);
	bzero(&args, sizeof (args));
	args.pathname = resolvepn.pn_path;
	/* don't free resolvepn until we are done with args */
	pn_free(&pn);

	/*
	 * Inform /proc that an exec() has started.
	 */
	prexecstart();

	/*
	 * Leave only the current lwp and force the other lwps
	 * to exit. Since exitlwps() waits until all other lwps are dead, if
	 * the calling process has an aslwp, all pending signals from it will
	 * be transferred to this process before continuing past this call.
	 */
	exitlwps(0);

	ASSERT((p->p_flag & ASLWP) == 0);
	ASSERT(p->p_aslwptp == NULL);

	/*
	 * revoke any doors created by the process.
	 */
	if (p->p_door_list)
		door_exit();

	/*
	 * release scheduler activations door (if any).
	 */
	if (p->p_sc_door) {
		VN_RELE(p->p_sc_door);
		p->p_sc_door = NULL;
	}

	/*
	 * Delete the dot4 timers.
	 */
	if (p->p_itimer != NULL)
		timer_exit();

	CPU_STAT_ADD_K(cpu_sysinfo.sysexec, 1);

	/*
	 * XXX - argh, it would be nice to get rid of execa completely
	 */
	ua.fname = fname;
	ua.argp = argp;
	ua.envp = envp;

	if (error = gexec(&vp, &ua, &args, NULL, 0, &execsz,
	    exec_file, p->p_cred))
		goto done;
	/*
	 * Free floating point registers (sun4u only)
	 */
	if (lwp)
		lwp_freeregs(lwp);
	/*
	 * Free device context
	 */
	if (curthread->t_ctx)
		freectx(curthread);

	up->u_execsz = execsz;	/* dependent portion should have checked */

	/*
	 * Remember file name for accounting.
	 */
	up->u_acflag &= ~AFORK;
	bcopy(exec_file, up->u_comm, MAXCOMLEN+1);

	/*
	 * Reset stack state to the user stack, clear set of signals
	 * caught on the signal stack, and reset list of signals that
	 * restart system calls; the new program's environment should
	 * not be affected by detritus from the old program. Any pending
	 * signals remain held, so don't clear p_hold and p_sig.
	 */
	mutex_enter(&p->p_lock);
	lwp->lwp_oldcontext = 0;
	sigemptyset(&up->u_signodefer);
	sigemptyset(&up->u_sigonstack);
	sigemptyset(&up->u_sigresethand);
	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;

	/*
	 * Make saved resource limit == current resource limit
	 * for file size. (See Large File Summit API)
	 */
	up->u_saved_rlimit.rlim_cur = up->u_rlimit[RLIMIT_FSIZE].rlim_cur;
	up->u_saved_rlimit.rlim_max = up->u_rlimit[RLIMIT_FSIZE].rlim_max;

	/*
	 * If the action was to catch the signal, then the action
	 * must be reset to SIG_DFL.
	 */
	for (i = 1; i < NSIG; i++) {
		if (up->u_signal[i - 1] != SIG_DFL &&
		    up->u_signal[i - 1] != SIG_IGN) {
			up->u_signal[i - 1] = SIG_DFL;
			sigemptyset(&up->u_sigmask[i - 1]);
			if (sigismember(&ignoredefault, i)) {
				sigdelq(p, NULL, i);
				sigdelq(p, p->p_tlist, i);
			}
		}
	}
	sigorset(&p->p_ignore, &ignoredefault);
	sigdiffset(&p->p_siginfo, &ignoredefault);
	sigdiffset(&p->p_sig, &ignoredefault);
	sigdiffset(&p->p_tlist->t_sig, &ignoredefault);
	p->p_flag &= ~(SNOWAIT|SJCTL|SWAITSIG);
	p->p_flag |= SEXECED;
	up->u_signal[SIGCLD - 1] = SIG_DFL;

	/*
	 * Reset lwp id and lpw count to default.
	 * This is a single-threaded process now.
	 */
	curthread->t_tid = 1;
	p->p_lwptotal = 1;
	p->p_lwpblocked = -1;

	/*
	 * Delete the dot4 sigqueues/signotifies.
	 */
	sigqfree(p);

	mutex_exit(&p->p_lock);

	/*
	 * Remove schedctl data.
	 */
	if (curthread->t_schedctl != NULL)
		schedctl_cleanup(curthread);

#ifdef i386
	/* If the process uses a private LDT then change it to default */
	if (p->p_ldt)
		ldt_free(p);
#endif

	/*
	 * Close all close-on-exec files.  Don't worry about locking u_lock
	 * when looking at u_nofiles because there should only be one lwp
	 * at this point.
	 */
	close_exec(p);
#ifdef TRACE
	trace_process_name((u_long) (p->p_pid), u.u_psargs);
#endif	/* TRACE */
	TRACE_3(TR_FAC_PROC, TR_PROC_EXEC, "proc_exec:pid %d as %x name %s",
		p->p_pid, p->p_as, up->u_psargs);
	setregs();
done:
	VN_RELE(vp);
	pn_free(&resolvepn);
	/*
	 * Inform /proc that the exec() has finished.
	 */
	prexecend();
	return (error);
}


/*
 * Perform generic exec duties and switchout to object-file specific
 * handler.
 */
int
gexec(
	struct vnode **vpp,
	struct execa *uap,
	struct uarg *args,
	struct intpdata *idatap,
	int level,
	long *execsz,
	caddr_t exec_file,
	struct cred *cred)
{
	struct vnode *vp;
	proc_t *pp = ttoproc(curthread);
	struct execsw *eswp;
	int error = 0;
	int nocd_flag = 0;
	ssize_t resid;
	uid_t uid, gid;
	struct vattr vattr;
	char magbuf[MAGIC_BYTES];
	int setid;
	struct cred *newcred = NULL;

	/*
	 * If we now have normal credentials (effective == real == saved),
	 * then turn off the NOCD flag, but remember the previous setting
	 * so we can restore it if we encounter an error.
	 */
	if (level == 0 && (pp->p_flag & NOCD) &&
	    cred->cr_uid == cred->cr_ruid && cred->cr_uid == cred->cr_suid &&
	    cred->cr_gid == cred->cr_rgid && cred->cr_gid == cred->cr_sgid) {
		nocd_flag = 1;
		mutex_enter(&pp->p_lock);
		pp->p_flag &= ~NOCD;
		mutex_exit(&pp->p_lock);
	}

	if ((error = execpermissions(*vpp, &vattr, args)) != 0)
		goto bad;

	/* need to open vnode for stateful file systems like rfs */
	if ((error = VOP_OPEN(vpp, FREAD, CRED())) != 0)
		goto bad;
	vp = *vpp;

	/*
	 * Note: to support binary compatibility with SunOS a.out
	 * executables, we read in the first four bytes, as the
	 * magic number is in bytes 2-3.
	 */
	if (error = vn_rdwr(UIO_READ, vp, magbuf, sizeof (magbuf),
	    (offset_t)0, UIO_SYSSPACE, 0, (rlim64_t)0, CRED(), &resid))
		goto bad;
	if (resid != 0)
		goto bad;

	if ((eswp = findexec_by_hdr(magbuf)) == NULL)
		goto bad;

	if (level == 0 && execsetid(vp, &vattr, &uid, &gid)) {
		/*
		 * if the suid/euid are not suser, check if
		 * cred will gain any new uid/gid from exec;
		 * if new id's, set a bit in p_flag for core()
		 */
		if (cred->cr_suid && cred->cr_uid) {
			if ((((vattr.va_mode & VSUID) &&
			    uid != cred->cr_suid && uid != cred->cr_uid)) ||
			    ((vattr.va_mode & VSGID) &&
			    gid != cred->cr_sgid && !groupmember(gid, cred))) {
				mutex_enter(&pp->p_lock);
				pp->p_flag |= NOCD;
				mutex_exit(&pp->p_lock);
			}
		}

		newcred = crdup(cred);
		newcred->cr_uid = uid;
		newcred->cr_gid = gid;
		newcred->cr_suid = uid;
		newcred->cr_sgid = gid;
		cred = newcred;
	}

	/* SunOS 4.x buy-back */
	if ((vp->v_vfsp->vfs_flag & VFS_NOSUID) &&
	    (vattr.va_mode & (VSUID|VSGID))) {
		cmn_err(CE_NOTE,
		    "!%s, uid %d: setuid execution not allowed, dev=%lx",
		    exec_file, cred->cr_uid, vp->v_vfsp->vfs_dev);
	}

	/*
	 * execsetid() told us whether or not we had to change the
	 * credentials of the process.  It did not tell us whether
	 * the executable is marked setid.  We determine that here.
	 */
	setid = (vp->v_vfsp->vfs_flag & VFS_NOSUID) == 0 &&
	    (vattr.va_mode & (VSUID|VSGID)) != 0;

	args->execswp = eswp; /* Save execsw pointer in uarg for exec_func */

	error = (*eswp->exec_func)(vp, uap, args, idatap, level,
	    execsz, setid, exec_file, cred);
	rw_exit(eswp->exec_lock);
	if (error != 0) {
		if (newcred != NULL)
			crfree(newcred);
		goto bad;
	}

	if (level == 0) {
		if (newcred != NULL) {
			/*
			 * Free the old credentials, and set the new ones.
			 * Do this for both the process and the (single) thread.
			 */
			crfree(pp->p_cred);
			pp->p_cred = cred;	/* cred already held for proc */
			crhold(cred);		/* hold new cred for thread */
			crfree(curthread->t_cred);
			curthread->t_cred = cred;
		}
		if (setid && (pp->p_flag & STRC) == 0) {
			/*
			 * If process is traced via /proc, arrange to
			 * invalidate the associated /proc vnode.
			 */
			if (pp->p_plist || (pp->p_flag & SPROCTR))
				args->traceinval = 1;
		}
		if (pp->p_flag & STRC)
			psignal(pp, SIGTRAP);
		if (args->traceinval)
			prinvalidate(&pp->p_user);
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	if (nocd_flag) {
		mutex_enter(&pp->p_lock);
		pp->p_flag |= NOCD;
		mutex_exit(&pp->p_lock);
	}
	return (error);
}

extern char *execswnames[];

struct execsw *
allocate_execsw(char *name, char *magic, size_t magic_size)
{
	int i, j;
	char *ename;
	char *magicp;

	mutex_enter(&execsw_lock);
	for (i = 0; i < nexectype; i++) {
		if (execswnames[i] == NULL) {
			ename = kmem_alloc(strlen(name) + 1, KM_SLEEP);
			(void) strcpy(ename, name);
			execswnames[i] = ename;
			/*
			 * Set the magic number last so that we
			 * don't need to hold the execsw_lock in
			 * findexectype().
			 */
			magicp = kmem_alloc(magic_size, KM_SLEEP);
			for (j = 0; j < magic_size; j++)
				magicp[j] = magic[j];
			execsw[i].exec_magic = magicp;
			mutex_exit(&execsw_lock);
			return (&execsw[i]);
		}
	}
	mutex_exit(&execsw_lock);
	return (NULL);
}

/*
 * Find the exec switch table entry with the corresponding magic string.
 */
struct execsw *
findexecsw(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0)
			return (eswp);
	}
	return (NULL);
}

/*
 * Find the execsw[] index for the given exec header string by looking for the
 * magic string at a specified offset and length for each kind of executable
 * file format until one matches.  If no execsw[] entry is found, try to
 * autoload a module for this magic string.
 */
struct execsw *
findexec_by_hdr(char *header)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (header && eswp->exec_maglen != 0 &&
		    bcmp(&header[eswp->exec_magoff], eswp->exec_magic,
			    eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

/*
 * Find the execsw[] index for the given magic string.  If no execsw[] entry
 * is found, try to autoload a module for this magic string.
 */
struct execsw *
findexec_by_magic(char *magic)
{
	struct execsw *eswp;

	for (eswp = execsw; eswp < &execsw[nexectype]; eswp++) {
		ASSERT(eswp->exec_maglen <= MAGIC_BYTES);
		if (magic && eswp->exec_maglen != 0 &&
		    bcmp(magic, eswp->exec_magic, eswp->exec_maglen) == 0) {
			if (hold_execsw(eswp) != 0)
				return (NULL);
			return (eswp);
		}
	}
	return (NULL);	/* couldn't find the type */
}

static int
hold_execsw(struct execsw *eswp)
{
	char *name;

	rw_enter(eswp->exec_lock, RW_READER);
	while (!LOADED_EXEC(eswp)) {
		rw_exit(eswp->exec_lock);
		name = execswnames[eswp-execsw];
		ASSERT(name);
		if (modload("exec", name) == -1)
			return (-1);
		rw_enter(eswp->exec_lock, RW_READER);
	}
	return (0);
}

static int
execsetid(struct vnode *vp, struct vattr *vattrp, uid_t *uidp, uid_t *gidp)
{
	proc_t *pp = ttoproc(curthread);
	uid_t uid, gid;

	/*
	 * Remember credentials.
	 */
	uid = pp->p_cred->cr_uid;
	gid = pp->p_cred->cr_gid;

	if ((vp->v_vfsp->vfs_flag & VFS_NOSUID) == 0) {
		if (vattrp->va_mode & VSUID)
			uid = vattrp->va_uid;
		if (vattrp->va_mode & VSGID)
			gid = vattrp->va_gid;
	}

	/*
	 * Set setuid/setgid protections if no ptrace() compatibility.
	 * For the super-user, honor setuid/setgid even in
	 * the presence of ptrace() compatibility.
	 */
	if (((pp->p_flag & STRC) == 0 || pp->p_cred->cr_uid == 0) &&
	    (pp->p_cred->cr_uid != uid ||
	    pp->p_cred->cr_gid != gid ||
	    pp->p_cred->cr_suid != uid ||
	    pp->p_cred->cr_sgid != gid)) {
		*uidp = uid;
		*gidp = gid;
		return (1);
	}
	return (0);
}

int
execpermissions(struct vnode *vp, struct vattr *vattrp, struct uarg *args)
{
	int error;
	proc_t *p = ttoproc(curthread);

	vattrp->va_mask = AT_MODE | AT_UID | AT_GID | AT_SIZE;
	if (error = VOP_GETATTR(vp, vattrp, ATTR_EXEC, p->p_cred))
		return (error);
	/*
	 * Check the access mode.
	 * If VPROC, ask /proc if the file is an object file.
	 */
	if ((error = VOP_ACCESS(vp, VEXEC, 0, p->p_cred)) != 0 ||
	    !(vp->v_type == VREG || (vp->v_type == VPROC && pr_isobject(vp))) ||
	    (vattrp->va_mode & (VEXEC|(VEXEC>>3)|(VEXEC>>6))) == 0) {
		if (error == 0)
			error = EACCES;
		return (error);
	}

	if ((p->p_plist || (p->p_flag & (STRC|SPROCTR))) &&
	    (error = VOP_ACCESS(vp, VREAD, 0, p->p_cred))) {
		/*
		 * If process is under ptrace(2) compatibility,
		 * fail the exec(2).
		 */
		if (p->p_flag & STRC)
			goto bad;
		/*
		 * Process is traced via /proc.
		 * Arrange to invalidate the /proc vnode.
		 */
		args->traceinval = 1;
	}
	return (0);
bad:
	if (error == 0)
		error = ENOEXEC;
	return (error);
}

/*
 * Map a section of an executable file into the user's
 * address space.
 */
int
execmap(struct vnode *vp, caddr_t addr, size_t len, size_t zfodlen,
    off_t offset, int prot, int page)
{
	int error = 0;
	off_t oldoffset;
	caddr_t zfodbase, oldaddr;
	size_t end, oldlen;
	size_t zfoddiff;
	label_t ljb;
	proc_t *p = ttoproc(curthread);

	oldaddr = addr;
	addr = (caddr_t)((uintptr_t)addr & PAGEMASK);
	if (len) {
		oldlen = len;
		len += ((size_t)oldaddr - (size_t)addr);
		oldoffset = offset;
		offset = (off_t)((uintptr_t)offset & PAGEMASK);
		if (page) {
			spgcnt_t  prefltmem, availm, npages;
			int preread;

			if (error = VOP_MAP(vp, (offset_t)offset,
			    p->p_as, &addr, len, prot, PROT_ALL,
			    MAP_PRIVATE | MAP_FIXED, CRED()))
				goto bad;

			/*
			 * If the segment can fit, then we prefault
			 * the entire segment in.  This is based on the
			 * model that says the best working set of a
			 * small program is all of its pages.
			 */
			npages = (spgcnt_t)btopr(len);
			prefltmem = freemem - desfree;
			preread =
			    (npages < prefltmem && len < PGTHRESH) ? 1 : 0;

			/*
			 * If we aren't prefaulting the segment,
			 * increment "deficit", if necessary to ensure
			 * that pages will become available when this
			 * process starts executing.
			 */
			availm = freemem - lotsfree;
			if (preread == 0 && npages > availm &&
			    deficit < lotsfree) {
				deficit += MIN((pgcnt_t)(npages - availm),
				    lotsfree - deficit);
			}

			if (preread) {
				TRACE_2(TR_FAC_PROC, TR_EXECMAP_PREREAD,
				    "execmap preread:freemem %d size %lu",
				    freemem, len);
				(void) as_fault(p->p_as->a_hat, p->p_as,
				    (caddr_t)addr, len, F_INVAL, S_READ);
			}
#ifdef TRACE
			else {
				TRACE_2(TR_FAC_PROC, TR_EXECMAP_NO_PREREAD,
				    "execmap no preread:freemem %d size %lu",
				    freemem, len);
			}
#endif
		} else {
			if (error = as_map(p->p_as, addr, len,
			    segvn_create, zfod_argsp))
				goto bad;
			/*
			 * Read in the segment in one big chunk.
			 */
			if (error = vn_rdwr(UIO_READ, vp, (caddr_t)oldaddr,
			    oldlen, (offset_t)oldoffset, UIO_USERSPACE, 0,
			    (rlim64_t)0, CRED(), (ssize_t *)0))
				goto bad;
			/*
			 * Now set protections.
			 */
			if (prot != PROT_ALL) {
				(void) as_setprot(p->p_as, (caddr_t)addr,
				    len, prot);
			}
		}
	}

	if (zfodlen) {
		end = (size_t)addr + len;
		zfodbase = (caddr_t)roundup(end, PAGESIZE);
		zfoddiff = (uintptr_t)zfodbase - end;
		if (zfoddiff) {
			if (on_fault(&ljb)) {
				error = EFAULT;
				goto bad;
			}
			(void) uzero((void *)end, zfoddiff);
			no_fault();
		}
		if (zfodlen > zfoddiff) {
			zfodlen -= zfoddiff;
			if (error = as_map(p->p_as, (caddr_t)zfodbase,
			    zfodlen, segvn_create, zfod_argsp))
				goto bad;
			if (prot != PROT_ALL) {
				(void) as_setprot(p->p_as, (caddr_t)zfodbase,
				    zfodlen, prot);
			}
		}
	}
	return (0);
bad:
	return (error);
}

void
setexecenv(struct execenv *ep)
{
	proc_t *p = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	struct vnode *vp;

	p->p_brkbase = ep->ex_brkbase;
	p->p_brksize = ep->ex_brksize;
	if (p->p_exec)
		VN_RELE(p->p_exec);	/* out with the old */
	vp = p->p_exec = ep->ex_vp;
	if (vp != NULL)
		VN_HOLD(vp);		/* in with the new */

	lwp->lwp_sigaltstack.ss_sp = 0;
	lwp->lwp_sigaltstack.ss_size = 0;
	lwp->lwp_sigaltstack.ss_flags = SS_DISABLE;

	p->p_user.u_execid = (int)ep->ex_magic;
}

int
execopen(struct vnode **vpp, int *fdp)
{
	struct vnode *vp = *vpp;
	file_t *fp;
	int error = 0;
	int filemode = FREAD;

	VN_HOLD(vp);		/* open reference */
	if (error = falloc(NULL, filemode, &fp, fdp)) {
		VN_RELE(vp);
		*fdp = -1;	/* just in case falloc changed value */
		return (error);
	}
	if (error = VOP_OPEN(&vp, filemode, CRED())) {
		VN_RELE(vp);
		setf(*fdp, NULLFP);
		unfalloc(fp);
		*fdp = -1;
		return (error);
	}
	*vpp = vp;		/* vnode should not have changed */
	fp->f_vnode = vp;
	mutex_exit(&fp->f_tlock);
	setf(*fdp, fp);
	return (0);
}

int
execclose(int fd)
{
	file_t *fp;

	fp = getandset(fd);
	if (fp == NULL)
		return (EBADF);

	return (closef(fp));
}


/*
 * noexec stub function.
 */
/*ARGSUSED*/
int
noexec(
    struct vnode *vp,
    struct execa *uap,
    struct uarg *args,
    struct intpdata *idatap,
    int level,
    long *execsz,
    int setid,
    caddr_t exec_file,
    struct cred *cred)
{
	cmn_err(CE_WARN, "missing exec capability for %s", uap->fname);
	return (ENOEXEC);
}
