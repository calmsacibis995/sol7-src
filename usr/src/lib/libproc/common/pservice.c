/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pservice.c	1.2	98/01/29 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/stack.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <stdarg.h>
#include "Pcontrol.h"

/*
 * This file implements the process services declared in <proc_service.h>.
 * This enables libproc to be used in conjunction with libthread_db
 * and librtld_db.
 */

#pragma weak ps_pdread = ps_pread
#pragma weak ps_ptread = ps_pread
#pragma weak ps_pdwrite = ps_pwrite
#pragma weak ps_ptwrite = ps_pwrite

ps_err_e
ps_pdmodel(struct ps_prochandle *P, int *modelp)
{
	*modelp = P->status.pr_dmodel;
	return (PS_OK);
}

ps_err_e
ps_pread(struct ps_prochandle *P, psaddr_t addr, void *buf, size_t size)
{
	if (pread(P->asfd, buf, size, (off_t)addr) != size)
		return (PS_BADADDR);
	return (PS_OK);
}

ps_err_e
ps_pwrite(struct ps_prochandle *P, psaddr_t addr, const void *buf, size_t size)
{
	if (pwrite(P->asfd, buf, size, (off_t)addr) != size)
		return (PS_BADADDR);
	return (PS_OK);
}

/*
 * libthread_db calls matched pairs of ps_pstop()/ps_pcontinue()
 * in the belief that the client may have left the process
 * running while calling in to the libthread_db interfaces.
 *
 * We interpret the meaning of these functions to be an inquiry
 * as to whether the process is stopped, not an action to be
 * performed to make it stopped.
 */
ps_err_e
ps_pstop(struct ps_prochandle *P)
{
	if (P->state != PS_STOP)
		return (PS_ERR);
	return (PS_OK);
}

ps_err_e
ps_pcontinue(struct ps_prochandle *P)
{
	if (P->state != PS_STOP)
		return (PS_ERR);
	return (PS_OK);
}

/*
 * ps_lstop() and ps_lcontinue() are not called by any code in libthread_db
 * or librtld_db.  We make them behave like ps_pstop() and ps_pcontinue().
 */
/* ARGSUSED1 */
ps_err_e
ps_lstop(struct ps_prochandle *P, lwpid_t lwpid)
{
	if (P->state != PS_STOP)
		return (PS_ERR);
	return (PS_OK);
}

/* ARGSUSED1 */
ps_err_e
ps_lcontinue(struct ps_prochandle *P, lwpid_t lwpid)
{
	if (P->state != PS_STOP)
		return (PS_ERR);
	return (PS_OK);
}

ps_err_e
ps_lgetregs(struct ps_prochandle *P, lwpid_t lwpid, prgregset_t regs)
{
	lwpstatus_t lwpstatus;
	char fname[64];
	int fd;

	/* can't get registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	/* we already have the registers of the representative lwp */
	if (P->status.pr_lwp.pr_lwpid == lwpid) {
		(void) memcpy(regs, P->status.pr_lwp.pr_reg,
			sizeof (prgregset_t));
		return (PS_OK);
	}

	/* not the representative lwp; we have to work harder */
	(void) sprintf(fname, "/proc/%d/lwp/%d/lwpstatus",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_RDONLY)) >= 0 &&
	    read(fd, &lwpstatus, sizeof (lwpstatus)) == sizeof (lwpstatus)) {
		(void) close(fd);
		(void) memcpy(regs, lwpstatus.pr_reg, sizeof (prgregset_t));
		return (PS_OK);
	}

	if (fd >= 0)
		(void) close(fd);
	return (PS_BADLID);
}

static int
writeregs(int ctlfd, const prgregset_t regs)
{
	long cmd = PCSREG;
	iovec_t iov[2];

	iov[0].iov_base = (caddr_t)&cmd;
	iov[0].iov_len = sizeof (long);
	iov[1].iov_base = (caddr_t)&regs[0];
	iov[1].iov_len = sizeof (prgregset_t);

	if (writev(ctlfd, iov, 2) < 0)
		return (-1);
	return (0);
}

ps_err_e
ps_lsetregs(struct ps_prochandle *P, lwpid_t lwpid, const prgregset_t regs)
{
	char fname[64];
	int fd;

	/* can't set registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	/* writing the process control file writes the representative lwp */
	if (P->status.pr_lwp.pr_lwpid == lwpid) {
		Psync(P);	/* consistency with the primary interfaces */
		if (writeregs(P->ctlfd, regs) != 0)
			return (PS_BADLID);
		(void) memcpy(P->status.pr_lwp.pr_reg, regs,
			sizeof (prgregset_t));
		return (PS_OK);
	}

	/* not the representative lwp; we have to work harder */
	(void) sprintf(fname, "/proc/%d/lwp/%d/lwpctl",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_WRONLY)) >= 0 &&
	    writeregs(fd, regs) == 0) {
		(void) close(fd);
		return (PS_OK);
	}

	if (fd >= 0)
		(void) close(fd);
	return (PS_BADLID);
}

ps_err_e
ps_lgetfpregs(struct ps_prochandle *P, lwpid_t lwpid, prfpregset_t *regs)
{
	lwpstatus_t lwpstatus;
	char fname[64];
	int fd;

	/* can't get registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	/* we already have the registers of the representative lwp */
	if (P->status.pr_lwp.pr_lwpid == lwpid) {
		(void) memcpy(regs, &P->status.pr_lwp.pr_fpreg,
			sizeof (prfpregset_t));
		return (PS_OK);
	}

	/* not the representative lwp; we have to work harder */
	(void) sprintf(fname, "/proc/%d/lwp/%d/lwpstatus",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_RDONLY)) >= 0 &&
	    read(fd, &lwpstatus, sizeof (lwpstatus)) == sizeof (lwpstatus)) {
		(void) close(fd);
		(void) memcpy(regs, &lwpstatus.pr_fpreg,
			sizeof (prfpregset_t));
		return (PS_OK);
	}

	if (fd >= 0)
		(void) close(fd);
	return (PS_BADLID);
}

static int
writefpregs(int ctlfd, const prfpregset_t *regs)
{
	long cmd = PCSFPREG;
	iovec_t iov[2];

	iov[0].iov_base = (caddr_t)&cmd;
	iov[0].iov_len = sizeof (long);
	iov[1].iov_base = (caddr_t)regs;
	iov[1].iov_len = sizeof (prfpregset_t);

	if (writev(ctlfd, iov, 2) < 0)
		return (-1);
	return (0);
}

ps_err_e
ps_lsetfpregs(struct ps_prochandle *P, lwpid_t lwpid, const prfpregset_t *regs)
{
	char fname[64];
	int fd;

	/* can't set registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	/* writing the process control file writes the representative lwp */
	if (P->status.pr_lwp.pr_lwpid == lwpid) {
		if (writefpregs(P->ctlfd, regs) != 0)
			return (PS_BADLID);
		(void) memcpy(&P->status.pr_lwp.pr_fpreg, regs,
			sizeof (prfpregset_t));
		return (PS_OK);
	}

	/* not the representative lwp; we have to work harder */
	(void) sprintf(fname, "/proc/%d/lwp/%d/lwpctl",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_WRONLY)) >= 0 &&
	    writefpregs(fd, regs) == 0) {
		(void) close(fd);
		return (PS_OK);
	}

	if (fd >= 0)
		(void) close(fd);
	return (PS_BADLID);
}

#if defined(sparc) || defined(__sparc)

ps_err_e
ps_lgetxregsize(struct ps_prochandle *P, lwpid_t lwpid, int *xrsize)
{
	char fname[64];
	struct stat statb;

	(void) sprintf(fname, "/proc/%d/lwp/%d/xregs",
		(int)P->status.pr_pid, (int)lwpid);

	if (stat(fname, &statb) != 0)
		return (PS_BADLID);

	*xrsize = (int)statb.st_size;
	return (PS_OK);
}

ps_err_e
ps_lgetxregs(struct ps_prochandle *P, lwpid_t lwpid, caddr_t xregs)
{
	char fname[64];
	int fd;

	/* can't get registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	(void) sprintf(fname, "/proc/%d/lwp/%d/xregs",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_RDONLY)) >= 0 &&
	    read(fd, xregs, sizeof (prxregset_t)) > 0) {
		(void) close(fd);
		return (PS_OK);
	}

	if (fd < 0)
		return (PS_BADLID);

	(void) close(fd);
	return (PS_ERR);
}

static int
writexregs(int ctlfd, caddr_t xregs)
{
	long cmd = PCSXREG;
	iovec_t iov[2];

	iov[0].iov_base = (caddr_t)&cmd;
	iov[0].iov_len = sizeof (long);
	iov[1].iov_base = (caddr_t)xregs;
	iov[1].iov_len = sizeof (prxregset_t);

	if (writev(ctlfd, iov, 2) < 0)
		return (-1);
	return (0);
}

ps_err_e
ps_lsetxregs(struct ps_prochandle *P, lwpid_t lwpid, caddr_t xregs)
{
	char fname[64];
	int fd;

	/* can't set registers unless the lwp is stopped */
	if (P->state != PS_STOP)
		return (PS_ERR);

	/* writing the process control file writes the representative lwp */
	if (P->status.pr_lwp.pr_lwpid == lwpid) {
		if (writexregs(P->ctlfd, xregs) != 0)
			return (PS_BADLID);
		return (PS_OK);
	}

	/* not the representative lwp; we have to work harder */
	(void) sprintf(fname, "/proc/%d/lwp/%d/lwpctl",
		(int)P->status.pr_pid, (int)lwpid);

	if ((fd = open(fname, O_WRONLY)) >= 0 &&
	    writexregs(fd, xregs) == 0) {
		(void) close(fd);
		return (PS_OK);
	}

	if (fd >= 0)
		(void) close(fd);
	return (PS_BADLID);
}

#endif	/* sparc */

#if defined(i386) || defined(__i386)

ps_err_e
ps_lgetLDT(struct ps_prochandle *P, lwpid_t lwpid, struct ssd *ldt)
{
	prgregset_t regs;
	char ldtfilename[64];
	struct stat statb;
	struct ssd *ldtarray;
	ps_err_e error;
	u_int gs;
	int fd;
	int nldt;
	int i;

	/*
	 * We need to get the ldt entry that matches the
	 * value in the lwp's GS register.
	 */
	if ((error = ps_lgetregs(P, lwpid, regs)) != PS_OK)
		return (error);
	gs = (int)regs[GS];

	(void) sprintf(ldtfilename, "/proc/%d/ldt", (int)P->pid);
	if ((fd = open(ldtfilename, O_RDONLY)) < 0 ||
	    fstat(fd, &statb) != 0 ||
	    statb.st_size < sizeof (struct ssd) ||
	    (ldtarray = malloc(statb.st_size)) == NULL ||
	    read(fd, ldtarray, statb.st_size) != statb.st_size) {
		if (fd >= 0)
			(void) close(fd);
		if (ldtarray)
			free(ldtarray);
		return (PS_ERR);
	}
	(void) close(fd);

	nldt = statb.st_size / sizeof (struct ssd);
	for (i = 0; i < nldt; i++) {
		if (gs == ldtarray[i].sel) {
			*ldt = ldtarray[i];
			break;
		}
	}
	free(ldtarray);

	if (i >= nldt)
		return (PS_ERR);
	return (PS_OK);
}

#endif	/* i386 */

#ifdef SOMEDAY_MAYBE_NEVER
/*
 * The very existence of these functions causes libthread_db
 * to create an "agent thread" in the target process.
 * The only way to turn off this behavior is to omit these functions.
 */
ps_err_e
ps_kill(struct ps_prochandle *P, int sig)
{
	struct {
		long cmd;
		long sig;
	} ctl;

	ctl.cmd = PCKILL;
	ctl.sig = sig;

	if (write(P->ctlfd, &ctl, sizeof (ctl)) != sizeof (ctl))
		return (PS_ERR);
	return (PS_OK);
}

/* ARGSUSED */
ps_err_e
ps_lrolltoaddr(struct ps_prochandle *P, lwpid_t lwpid,
	psaddr_t go_addr, psaddr_t stop_addr)
{
	return (PS_ERR);
}
#endif	/* SOMEDAY_MAYBE_NEVER */

/*
 * libthread_db doesn't call this function.
 * librtld_db does, but do we care?
 * Compromise: log to stderr if _libproc_debug is non-zero.
 */
void
ps_plog(const char *fmt, ...)
{
	va_list		ap;

	va_start(ap, fmt);
	if (_libproc_debug)
		(void) vfprintf(stderr, fmt, ap);
	va_end(ap);
}
