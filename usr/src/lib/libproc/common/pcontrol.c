/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pcontrol.c	1.3	98/02/17 SMI"

#define	_SYSCALL32

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/stack.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include "Pcontrol.h"

int	_libproc_debug;		/* set non-zero to enable debugging printfs */

/*
 * Function prototypes for static routines in this module.
 */
static	int	Pstopstatus(struct ps_prochandle *, long, uint32_t);
static	void	deadcheck(struct ps_prochandle *);
static	void	restore_tracing_flags(struct ps_prochandle *);
static	int	execute(struct ps_prochandle *, int);
static	int	checksyscall(struct ps_prochandle *);

/*
 * This is the library's .init handler.
 */
#pragma init(_libproc_init)
void
_libproc_init()
{
	const char *str = getenv("LIBPROC_DEBUG");

	if (str == NULL)
		_libproc_debug = 0;
	else
		_libproc_debug = atoi(str);
}

static int
dupfd(int fd, int dfd)
{
	/*
	 * Make sure fd not one of 0, 1, or 2.
	 * This allows the program to work when spawned by init(1m).
	 * Also, if dfd is non-zero, dup the fd to be dfd.
	 */
	if (dfd > 0 || (0 <= fd && fd <= 2)) {
		if (dfd <= 0)
			dfd = 3;
		dfd = fcntl(fd, F_DUPFD, dfd);
		(void) close(fd);
		fd = dfd;
	}
	/*
	 * Mark it close-on-exec so any created process doesn't inherit it.
	 */
	if (fd >= 0)
		(void) fcntl(fd, F_SETFD, FD_CLOEXEC);
	return (fd);
}

static const sigset_t *
blockable_signals(void)
{
	static int initialized = 0;
	static sigset_t blockable;

	if (!initialized) {
		(void) sigfillset(&blockable);
		(void) sigdelset(&blockable, SIGKILL);
		(void) sigdelset(&blockable, SIGSTOP);
	}

	return (&blockable);
}

/*
 * Create a new controlled process.
 * Leave it stopped on successful exit from exec() or execve().
 * Return an opaque pointer to its process control structure.
 * Return NULL if process cannot be created (fork()/exec() not successful).
 */
struct ps_prochandle *
Pcreate(const char *file,	/* executable file name */
	char *const *argv,	/* argument vector */
	int *perr,	/* pointer to error return code */
	char *path,	/* if non-null, holds exec path name on return */
	size_t len)	/* size of the path buffer */
{
	char execpath[PATH_MAX];
	char procname[100];
	struct ps_prochandle *P;
	pid_t pid;
	int fd;
	char *fname;
	int rc;

	if (len == 0)	/* zero length, no path */
		path = NULL;
	if (path != NULL)
		*path = '\0';

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		perror("Pcreate malloc()");
		*perr = C_STRANGE;
		return (NULL);
	}

	if ((pid = fork()) == -1) {
		free(P);
		*perr = C_FORK;
		return (NULL);
	}

	if (pid == 0) {			/* child process */
		id_t id;

		(void) pause();		/* wait for PRSABORT from parent */

		/*
		 * If running setuid or setgid, reset credentials to normal.
		 */
		if ((id = getgid()) != getegid())
			(void) setgid(id);
		if ((id = getuid()) != geteuid())
			(void) setuid(id);

		(void) execvp(file, argv);	/* execute the program */
		_exit(127);
	}

	/*
	 * Initialize the process structure.
	 */
	(void) memset(P, 0, sizeof (*P));
	P->flags |= CREATED;
	P->state = PS_RUN;
	P->pid = pid;
	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;
	Pinitsym(P);

	/*
	 * Open the /proc/pid files.
	 */
	(void) sprintf(procname, "/proc/%d/", (int)pid);
	fname = procname + strlen(procname);

	/*
	 * Exclusive write open advises others not to interfere.
	 * There is no reason for any of these open()s to fail.
	 */
	(void) strcpy(fname, "as");
	if ((fd = open(procname, (O_RDWR|O_EXCL))) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"as\")");
		rc = C_STRANGE;
		goto bad;
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"status\")");
		rc = C_STRANGE;
		goto bad;
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		perror("Pcreate open(\"ctl\")");
		rc = C_STRANGE;
		goto bad;
	}
	P->ctlfd = fd;

	(void) Pstop(P, 0);	/* stop the controlled process */

	/*
	 * Wait for process to sleep in pause().
	 * We use PCRUN with the PRSTOP flag to make incremental
	 * progress until the sleep in pause() state is reached.
	 * There is no reason for this to fail other than an interrupt.
	 */
	for (;;) {
		if (P->state == PS_STOP &&
		    P->status.pr_lwp.pr_why == PR_REQUESTED &&
		    (P->status.pr_flags & PR_ASLEEP) &&
		    P->status.pr_lwp.pr_syscall == SYS_pause)
			break;

		if (P->state != PS_STOP ||	/* interrupt or process died */
		    Psetrun(P, 0, PRSTOP) != 0) {	/* can't restart */
			if (errno == EINTR || errno == ERESTART)
				rc = C_INTR;
			else {
				perror("Pcreate PCRUN");
				rc = C_STRANGE;
			}
			goto bad;
		}

		(void) Pwait(P, 0);
	}

	/*
	 * Kick the process off the pause() and catch
	 * it again on entry to exec() or exit().
	 */
	(void) Psysentry(P, SYS_exit, 1);
	(void) Psysentry(P, SYS_exec, 1);
	(void) Psysentry(P, SYS_execve, 1);
	if (Psetrun(P, 0, PRSABORT) == -1) {
		perror("Pcreate PCRUN");
		rc = C_STRANGE;
		goto bad;
	}
	(void) Pwait(P, 0);
	if (P->state != PS_STOP) {
		perror("Pcreate PCWSTOP");
		rc = C_STRANGE;
		goto bad;
	}

	/*
	 * Move the process through instances of failed exec()s
	 * to reach the point of stopped on successful exec().
	 */
	(void) Psysexit(P, SYS_exec, TRUE);
	(void) Psysexit(P, SYS_execve, TRUE);

	while (P->state == PS_STOP &&
	    P->status.pr_lwp.pr_why == PR_SYSENTRY &&
	    (P->status.pr_lwp.pr_what == SYS_execve ||
	    P->status.pr_lwp.pr_what == SYS_exec)) {
		/*
		 * Fetch the exec path name now, before we complete
		 * the exec().  We may lose the process and be unable
		 * to get the information later.
		 */
		(void) proc_read_string(P->asfd, execpath, sizeof (execpath),
			(off_t)P->status.pr_lwp.pr_sysarg[0]);
		if (path != NULL)
			(void) strncpy(path, execpath, len);
		/*
		 * Set the process running and wait for
		 * it to stop on exit from the exec().
		 */
		(void) Psetrun(P, 0, 0);
		(void) Pwait(P, 0);

		if (P->state == PS_LOST &&		/* we lost control */
		    Preopen(P) != 0) {		/* and we can't get it back */
			rc = C_PERM;
			goto bad;
		}

		/*
		 * If the exec() failed, continue the loop, expecting
		 * there to be more attempts to exec(), based on PATH.
		 */
		if (P->state == PS_STOP &&
		    P->status.pr_lwp.pr_why == PR_SYSEXIT &&
		    (P->status.pr_lwp.pr_what == SYS_execve ||
		    P->status.pr_lwp.pr_what == SYS_exec) &&
		    P->status.pr_lwp.pr_errno) {
			/*
			 * The exec() failed.  Set the process running and
			 * wait for it to stop on entry to the next exec().
			 */
			(void) Psetrun(P, 0, 0);
			(void) Pwait(P, 0);
			continue;
		}
		break;
	}

	if (P->state == PS_STOP &&
	    P->status.pr_lwp.pr_why == PR_SYSEXIT &&
	    (P->status.pr_lwp.pr_what == SYS_execve ||
	    P->status.pr_lwp.pr_what == SYS_exec) &&
	    P->status.pr_lwp.pr_errno == 0) {
		/*
		 * The process is stopped on successful exec() or execve().
		 * Turn off all tracing flags and return success.
		 */
		restore_tracing_flags(P);
#ifndef _LP64
		/* We must be a 64-bit process to deal with a 64-bit process */
		if (P->status.pr_dmodel == PR_MODEL_LP64) {
			rc = C_LP64;
			goto bad;
		}
#endif
		/*
		 * Set run-on-last-close so the controlled process
		 * runs even if we die on a signal.
		 */
		(void) Psetflags(P, PR_RLC);
		*perr = 0;
		return (P);
	}
	rc = C_NOEXEC;

bad:
	(void) kill(pid, SIGKILL);
	if (path != NULL && rc != C_PERM && rc != C_LP64)
		*path = '\0';
	Pfree(P);
	*perr = rc;
	return (NULL);
}

/*
 * Return a printable string corresponding to a Pcreate() error return.
 */
const char *
Pcreate_error(int error)
{
	const char *str;

	switch (error) {
	case C_FORK:		/* Unable to fork */
		str = "cannot fork";
		break;
	case C_PERM:		/* No permission (file set-id or unreadable) */
		str = "file is set-id or unreadable";
		break;
	case C_NOEXEC:		/* Cannot find executable file */
		str = "cannot find executable file";
		break;
	case C_INTR:		/* Interrupt received while creating */
		str = "operation interrupted";
		break;
	case C_LP64:		/* Program is _LP64, self is _ILP32 */
		str = "program is _LP64, self is not";
		break;
	case C_STRANGE:		/* Unanticipated error, perror() was called */
	default:
		str = "unknown error";
		break;
	}

	return (str);
}

/*
 * Grab an existing process.
 * Return an opaque pointer to its process control structure.
 *
 * pid:		UNIX process ID.
 * flags:
 *	PGRAB_RETAIN	Retain tracing flags (default clears all tracing flags).
 *	PGRAB_FORCE	Grab regardless of whether process is already traced.
 * perr:	pointer to error return code.
 */
struct ps_prochandle *
Pgrab(pid_t pid, int flags, int *perr)
{
	struct ps_prochandle *P;
	int fd;
	uid_t ruid;
	struct prcred prcred;
	char procname[100];
	char *fname;
	int rc = 0;

	if (flags & ~(PGRAB_RETAIN|PGRAB_FORCE)) {
		(void) fprintf(stderr, "Pgrab(): bad flags: 0x%x\n", flags);
		*perr = G_STRANGE;
		return (NULL);
	}

	if ((P = malloc(sizeof (struct ps_prochandle))) == NULL) {
		perror("Pgrab malloc()");
		*perr = G_STRANGE;
		return (NULL);
	}
	P->asfd = -1;
	P->ctlfd = -1;
	P->statfd = -1;

again:	/* Come back here if we lose it in the Window of Vulnerability */
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;
	Pinitsym(P);

	/* open the /proc/pid files */
	(void) sprintf(procname, "/proc/%d/", (int)pid);
	fname = procname + strlen(procname);

	/*
	 * Request exclusive open to avoid grabbing someone else's
	 * process and to prevent others from interfering afterwards.
	 * If this fails and the 'PGRAB_FORCE' flag is set, attempt to
	 * open non-exclusively.
	 */
	(void) strcpy(fname, "as");
	if (((fd = open(procname, (O_RDWR|O_EXCL))) < 0 &&
	    (fd = ((flags & PGRAB_FORCE)? open(procname, O_RDWR) : -1)) < 0) ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		case EACCES:
		case EPERM:
			rc = G_PERM;
			break;
		case EBUSY:
			if (!(flags & PGRAB_FORCE) || geteuid() != 0) {
				rc = G_BUSY;
				break;
			}
			/* FALLTHROUGH */
		default:
			perror("Pgrab open(\"as\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		default:
			perror("Pgrab open(\"status\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0) {
		switch (errno) {
		case ENOENT:
			rc = G_NOPROC;
			break;
		default:
			perror("Pgrab open(\"ctl\")");
			rc = G_STRANGE;
			break;
		}
		goto err;
	}
	P->ctlfd = fd;

	P->state = PS_RUN;
	P->pid = pid;

	/*
	 * We are now in the Window of Vulnerability (WoV).
	 * The process may exec() a setuid/setgid or unreadable
	 * object file between the open() and the PCSTOP.
	 * We will get EAGAIN in this case and must start over.
	 */

	/*
	 * If the process is a system process, we can't control it
	 * even if we are super-user.  First get the status structure.
	 */
	if (Pstopstatus(P, PCNULL, 0) != 0) {
		if (P->state == PS_LOST)	/* WoV */
			goto again;
		if (P->state == PS_DEAD)	/* No complaint about zombies */
			rc = G_NOPROC;
		else {
			perror("Pgrab read(\"status\")");
			rc = G_STRANGE;
		}
		goto err;
	}
	if (P->status.pr_flags & PR_ISSYS) {
		rc = G_SYS;
		goto err;
	}
#ifndef _LP64
	/* We must be a 64-bit process to deal with a 64-bit process */
	if (P->status.pr_dmodel == PR_MODEL_LP64) {
		rc = G_LP64;
		goto err;
	}
#endif

	/*
	 * Remember the status for use by Prelease().
	 */
	P->orig_status = P->status;	/* structure copy */

	/*
	 * Verify process credentials in case we are running setuid root.
	 * We only verify that our real uid matches the process's real uid.
	 * This means that the user really did create the process, even
	 * if using a different group id (via newgrp(1) for example).
	 */
	if (proc_get_cred(P->pid, &prcred, 0) < 0) {
		if (errno == EAGAIN)	/* WoV */
			goto again;
		if (errno == ENOENT)	/* No complaint about zombies */
			rc = G_NOPROC;
		else {
			perror("Pgrab credentials");
			rc = G_STRANGE;
		}
		goto err;
	}
	if ((ruid = getuid()) != 0 &&	/* super-user allowed anything */
	    ruid != prcred.pr_ruid) {	/* credentials check failed */
		errno = EACCES;
		rc = G_PERM;
		goto err;
	}

	/*
	 * Before stopping the process, make sure it's not ourself.
	 */
	if (pid == getpid()) {
		/*
		 * Verify that the process is really ourself:
		 * Set a magic number, read it through the
		 * /proc file and see if the results match.
		 */
		uint32_t magic1 = 0;
		uint32_t magic2 = 2;

		errno = 0;

		if (Pread(P, &magic2, sizeof (magic2), (uintptr_t)&magic1)
		    == sizeof (magic2) &&
		    magic2 == 0 &&
		    (magic1 = 0xfeedbeef) &&
		    Pread(P, &magic2, sizeof (magic2), (uintptr_t)&magic1)
		    == sizeof (magic2) &&
		    magic2 == 0xfeedbeef) {
			rc = G_SELF;
			goto err;
		}
	}

	/*
	 * If the process is already stopped or has been directed
	 * to stop via /proc, do not set run-on-last-close.
	 */
	if (!(P->status.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP))) {
		/*
		 * Mark the process run-on-last-close so
		 * it runs even if we die from SIGKILL.
		 */
		if (Psetflags(P, PR_RLC) != 0) {
			if (errno == EAGAIN)	/* WoV */
				goto again;
			if (errno == ENOENT)	/* No complaint about zombies */
				rc = G_ZOMB;
			else {
				perror("Pgrab set PR_RLC");
				rc = G_STRANGE;
			}
			goto err;
		}

		/*
		 * Stop the process, get its status and signal/syscall masks.
		 */
		if ((P->status.pr_lwp.pr_why == PR_JOBCONTROL &&
		    Pstopstatus(P, PCDSTOP, 0) != 0) ||
		    Pstopstatus(P, PCSTOP, 2000) != 0) {
			if (P->state == PS_LOST)	/* WoV */
				goto again;
			if ((errno != EINTR && errno != ERESTART) ||
			    (P->state != PS_STOP &&
			    !(P->status.pr_flags & PR_DSTOP))) {
				if (P->state != PS_RUN && errno != ENOENT) {
					perror("Pgrab PCSTOP");
					rc = G_STRANGE;
				} else {
					rc = G_ZOMB;
				}
				goto err;
			}
		}
	}

	/*
	 * Process should either be stopped via /proc or
	 * there should be an outstanding stop directive.
	 */
	if ((P->status.pr_flags & (PR_ISTOP|PR_DSTOP)) == 0) {
		(void) fprintf(stderr, "Pgrab: process is not stopped\n");
		rc = G_STRANGE;
		goto err;
	}

#ifndef _LP64
	/* Test this again now because the process might have done an exec() */
	if (P->status.pr_dmodel == PR_MODEL_LP64) {
		rc = G_LP64;
		goto err;
	}
#endif

	/*
	 * Cancel all tracing flags unless the PGRAB_RETAIN flag is set.
	 */
	if (!(flags & PGRAB_RETAIN)) {
		(void) Psysentry(P, 0, FALSE);
		(void) Psysexit(P, 0, FALSE);
		(void) Psignal(P, 0, FALSE);
		(void) Pfault(P, 0, FALSE);
		Psync(P);
	}

	*perr = 0;
	return (P);

err:
	Pfree(P);
	*perr = rc;
	return (NULL);
}

/*
 * Return a printable string corresponding to a Pgrab() error return.
 */
const char *
Pgrab_error(int error)
{
	const char *str;

	switch (error) {
	case G_NOPROC:		/* No such process */
		str = "nonexistent process";
		break;
	case G_ZOMB:		/* Zombie process */
		str = "zombie process";
		break;
	case G_PERM:		/* No permission */
		str = "permission denied";
		break;
	case G_BUSY:		/* Another process has control */
		str = "process is traced";
		break;
	case G_SYS:		/* System process */
		str = "system process";
		break;
	case G_SELF:		/* Process is self */
		str = "attempt to grab self";
		break;
	case G_INTR:		/* Interrupt received while grabbing */
		str = "operation interrupted";
		break;
	case G_LP64:		/* Process is _LP64, self is _ILP32 */
		str = "process is _LP64, self is not";
		break;
	case G_STRANGE:		/* Unanticipated error, perror() was called */
	default:
		str = "unknown error";
		break;
	}

	return (str);
}

/*
 * Free a process control structure.
 * Close the file descriptors but don't do the Prelease logic.
 */
void
Pfree(struct ps_prochandle *P)
{
	if (P->agentctlfd >= 0)
		(void) close(P->agentctlfd);
	if (P->agentstatfd >= 0)
		(void) close(P->agentstatfd);
	if (P->ctlfd >= 0)
		(void) close(P->ctlfd);
	if (P->asfd >= 0)
		(void) close(P->asfd);
	if (P->statfd >= 0)
		(void) close(P->statfd);
	proc_reset_maps(P);

	/* clear out the structure as a precaution against reuse */
	(void) memset(P, 0, sizeof (*P));
	P->ctlfd = -1;
	P->asfd = -1;
	P->statfd = -1;
	P->agentctlfd = -1;
	P->agentstatfd = -1;

	free(P);
}

/*
 * Return the state of the process, one of the PS_* values.
 */
int
Pstate(struct ps_prochandle *P)
{
	return (P->state);
}

/*
 * Return the open address space file descriptor for the process.
 * Clients must not close this file descriptor, not use it
 * after the process is freed.
 */
int
Pasfd(struct ps_prochandle *P)
{
	return (P->asfd);
}

/*
 * Return the open control file descriptor for the process.
 * Clients must not close this file descriptor, not use it
 * after the process is freed.
 */
int
Pctlfd(struct ps_prochandle *P)
{
	return (P->ctlfd);
}

/*
 * Return a pointer to the process status structure.
 * Clients should not hold on to this pointer indefinitely.
 * It will become invalid on Prelease().
 */
pstatus_t *
Pstatus(struct ps_prochandle *P)
{
	return (&P->status);
}

/*
 * Ensure that all cached state is written to the process.
 * The cached state is the lwp's signal mask and registers
 * and the process's tracing flags.
 */
void
Psync(struct ps_prochandle *P)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long cmd[6];
	iovec_t iov[12];
	int n = 0;

	if (P->flags & SETHOLD) {
		cmd[0] = PCSHOLD;
		iov[n].iov_base = (caddr_t)&cmd[0];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_lwp.pr_lwphold;
		iov[n++].iov_len = sizeof (P->status.pr_lwp.pr_lwphold);
	}
	if (P->flags & SETREGS) {
		cmd[1] = PCSREG;
		iov[n].iov_base = (caddr_t)&cmd[1];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->REG[0];
		iov[n++].iov_len = sizeof (P->REG);
	}
	if (P->flags & SETSIG) {
		cmd[2] = PCSTRACE;
		iov[n].iov_base = (caddr_t)&cmd[2];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_sigtrace;
		iov[n++].iov_len = sizeof (P->status.pr_sigtrace);
	}
	if (P->flags & SETFAULT) {
		cmd[3] = PCSFAULT;
		iov[n].iov_base = (caddr_t)&cmd[3];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_flttrace;
		iov[n++].iov_len = sizeof (P->status.pr_flttrace);
	}
	if (P->flags & SETENTRY) {
		cmd[4] = PCSENTRY;
		iov[n].iov_base = (caddr_t)&cmd[4];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_sysentry;
		iov[n++].iov_len = sizeof (P->status.pr_sysentry);
	}
	if (P->flags & SETEXIT) {
		cmd[5] = PCSEXIT;
		iov[n].iov_base = (caddr_t)&cmd[5];
		iov[n++].iov_len = sizeof (long);
		iov[n].iov_base = (caddr_t)&P->status.pr_sysexit;
		iov[n++].iov_len = sizeof (P->status.pr_sysexit);
	}

	if (n == 0 || writev(ctlfd, iov, n) < 0)
		return;		/* nothing to do or write failed */

	P->flags &= ~(SETSIG|SETFAULT|SETENTRY|SETEXIT|SETHOLD|SETREGS);
}

/*
 * Create the /proc agent lwp for further operations.
 */
int
Pcreate_agent(struct ps_prochandle *P)
{
	int fd;
	char pathname[100];
	char *fname;
	struct {
		long	cmd;
		prgregset_t regs;
	} cmd;

	/*
	 * Make another reference.
	 * If not first reference, we already have the /proc agent lwp active.
	 */
	if (P->agentcnt > 0) {
		P->agentcnt++;
		return (0);
	}

	/*
	 * Create the special /proc agent lwp.
	 * Give it the registers of the representative lwp.
	 */
	Psync(P);
	cmd.cmd = PCAGENT;
	(void) memcpy((char *)&cmd.regs, (char *)&P->REG[0], sizeof (P->REG));
	if (write(P->ctlfd, &cmd, sizeof (cmd)) != sizeof (cmd))
		goto bad;
	/* refresh the process status */
	(void) Pstopstatus(P, PCNULL, 0);

	/* open the agent lwp files */
	(void) sprintf(pathname, "/proc/%d/lwp/agent/", (int)P->pid);
	fname = pathname + strlen(pathname);

	/*
	 * It is difficult to know how to recover from the two errors
	 * that follow.  The agent lwp exists and we need to kill it,
	 * but we can't because we need it active in order to kill it.
	 * We just hope that these failures never occur.
	 */
	(void) strcpy(fname, "lwpstatus");
	if ((fd = open(pathname, O_RDONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0)
		goto bad;
	P->agentstatfd = fd;

	(void) strcpy(fname, "lwpctl");
	if ((fd = open(pathname, O_WRONLY)) < 0 ||
	    (fd = dupfd(fd, 0)) < 0)
		goto bad;
	P->agentctlfd = fd;

	/* get the agent lwp status */
	P->agentcnt++;
	if (Pstopstatus(P, PCNULL, 0) != 0) {
		Pdestroy_agent(P);
		return (-1);
	}

	return (0);

bad:
	if (P->agentstatfd >= 0)
		(void) close(P->agentstatfd);
	if (P->agentctlfd >= 0)
		(void) close(P->agentctlfd);
	P->agentstatfd = -1;
	P->agentctlfd = -1;
	/* refresh the process status */
	(void) Pstopstatus(P, PCNULL, 0);
	return (-1);
}

/* undo Pcreate_agent() */
void
Pdestroy_agent(struct ps_prochandle *P)
{
	/*
	 * Decrement the /proc agent agent reference count.
	 * On last reference, destroy the agent.
	 */
	if (P->agentcnt > 1)
		P->agentcnt--;
	else {
		Psync(P);
		(void) pr_lwp_exit(P);
		(void) close(P->agentctlfd);
		(void) close(P->agentstatfd);
		P->agentctlfd = -1;
		P->agentstatfd = -1;
		P->agentcnt = 0;
		/* refresh the process status */
		(void) Pstopstatus(P, PCNULL, 0);
	}
}

/*
 * Reopen the /proc file (after PS_LOST).
 */
int
Preopen(struct ps_prochandle *P)
{
	int fd;
	char procname[100];
	char *fname;

	if (P->agentcnt > 0) {
		P->agentcnt = 1;
		Pdestroy_agent(P);
	}

	(void) sprintf(procname, "/proc/%d/", (int)P->pid);
	fname = procname + strlen(procname);

	(void) strcpy(fname, "as");
	if ((fd = open(procname, O_RDWR)) < 0 ||
	    close(P->asfd) < 0 ||
	    (fd = dupfd(fd, P->asfd)) != P->asfd) {
		if (_libproc_debug)
			perror("Preopen open(\"as\")");
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	P->asfd = fd;

	(void) strcpy(fname, "status");
	if ((fd = open(procname, O_RDONLY)) < 0 ||
	    close(P->statfd) < 0 ||
	    (fd = dupfd(fd, P->statfd)) != P->statfd) {
		if (_libproc_debug)
			perror("Preopen open(\"status\")");
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	P->statfd = fd;

	(void) strcpy(fname, "ctl");
	if ((fd = open(procname, O_WRONLY)) < 0 ||
	    close(P->ctlfd) < 0 ||
	    (fd = dupfd(fd, P->ctlfd)) != P->ctlfd) {
		if (_libproc_debug)
			perror("Preopen open(\"ctl\")");
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}
	P->ctlfd = fd;

	/*
	 * The process should be stopped on exec (REQUESTED)
	 * or else should be stopped on exit from exec() (SYSEXIT)
	 */
	P->state = PS_RUN;
	if (Pwait(P, 0) == 0 &&
	    P->state == PS_STOP &&
	    (P->status.pr_lwp.pr_why == PR_REQUESTED ||
	    (P->status.pr_lwp.pr_why == PR_SYSEXIT &&
	    (P->status.pr_lwp.pr_what == SYS_exec ||
	    P->status.pr_lwp.pr_what == SYS_execve)))) {
		/* fake up stop-on-exit-from-execve */
		if (P->status.pr_lwp.pr_why == PR_REQUESTED) {
			P->status.pr_lwp.pr_why = PR_SYSEXIT;
			P->status.pr_lwp.pr_what = SYS_execve;
		}
	} else {
		(void) fprintf(stderr,
		"Preopen: expected REQUESTED or SYSEXIT(SYS_execve) stop\n");
	}

	return (0);
}

/*
 * Define all settable flags other than the microstate accounting flags.
 */
#define	ALL_SETTABLE_FLAGS (PR_FORK|PR_RLC|PR_KLC|PR_ASYNC|PR_BPTADJ|PR_PTRACE)

/*
 * Restore /proc tracing flags to their original values
 * in preparation for releasing the process.
 * Also called by Pcreate() to clear all tracing flags.
 */
static void
restore_tracing_flags(struct ps_prochandle *P)
{
	long flags;
	long cmd[4];
	iovec_t iov[8];

	if (P->flags & CREATED) {
		/* we created this process; clear all tracing flags */
		premptyset(&P->status.pr_sigtrace);
		premptyset(&P->status.pr_flttrace);
		premptyset(&P->status.pr_sysentry);
		premptyset(&P->status.pr_sysexit);
		if ((P->status.pr_flags & ALL_SETTABLE_FLAGS) != 0)
			(void) Punsetflags(P, ALL_SETTABLE_FLAGS);
	} else {
		/* we grabbed the process; restore its tracing flags */
		P->status.pr_sigtrace = P->orig_status.pr_sigtrace;
		P->status.pr_flttrace = P->orig_status.pr_flttrace;
		P->status.pr_sysentry = P->orig_status.pr_sysentry;
		P->status.pr_sysexit  = P->orig_status.pr_sysexit;
		if ((P->status.pr_flags & ALL_SETTABLE_FLAGS) !=
		    (flags = (P->orig_status.pr_flags & ALL_SETTABLE_FLAGS))) {
			(void) Punsetflags(P, ALL_SETTABLE_FLAGS);
			if (flags)
				(void) Psetflags(P, flags);
		}
	}

	cmd[0] = PCSTRACE;
	iov[0].iov_base = (caddr_t)&cmd[0];
	iov[0].iov_len = sizeof (long);
	iov[1].iov_base = (caddr_t)&P->status.pr_sigtrace;
	iov[1].iov_len = sizeof (P->status.pr_sigtrace);

	cmd[1] = PCSFAULT;
	iov[2].iov_base = (caddr_t)&cmd[1];
	iov[2].iov_len = sizeof (long);
	iov[3].iov_base = (caddr_t)&P->status.pr_flttrace;
	iov[3].iov_len = sizeof (P->status.pr_flttrace);

	cmd[2] = PCSENTRY;
	iov[4].iov_base = (caddr_t)&cmd[2];
	iov[4].iov_len = sizeof (long);
	iov[5].iov_base = (caddr_t)&P->status.pr_sysentry;
	iov[5].iov_len = sizeof (P->status.pr_sysentry);

	cmd[3] = PCSEXIT;
	iov[6].iov_base = (caddr_t)&cmd[3];
	iov[6].iov_len = sizeof (long);
	iov[7].iov_base = (caddr_t)&P->status.pr_sysexit;
	iov[7].iov_len = sizeof (P->status.pr_sysexit);

	(void) writev(P->ctlfd, iov, 8);

	P->flags &= ~(SETSIG|SETFAULT|SETENTRY|SETEXIT);
}

/*
 * Release the process.  Frees the process control structure.
 * flags:
 *	PRELEASE_CLEAR	Clear all tracing flags.
 *	PRELEASE_RETAIN	Retain current tracing flags.
 *	PRELEASE_HANG	Leave the process stopped and abandoned.
 *	PRELEASE_KILL	Terminate the process with SIGKILL.
 */
void
Prelease(struct ps_prochandle *P, int flags)
{
	if (flags &
	    ~(PRELEASE_CLEAR|PRELEASE_RETAIN|PRELEASE_HANG|PRELEASE_KILL))
		(void) fprintf(stderr, "Prelease(): bad flags: 0x%x\n", flags);

	if (_libproc_debug)
		(void) fprintf(stderr, "Prelease: releasing pid # %d\n",
			(int)P->pid);

	if (P->agentcnt > 0) {
		P->agentcnt = 1;
		Pdestroy_agent(P);
	}

	/*
	 * Attempt to stop the process.
	 */
	if (P->state == PS_RUN)
		(void) Pstop(P, 1000);

	if (flags & PRELEASE_KILL) {
		/* Terminate the process with SIGKILL */
		if (P->state == PS_STOP)
			(void) Psetrun(P, SIGKILL, 0);
		(void) kill(P->pid, SIGKILL);
		Pfree(P);
		return;
	}

	/*
	 * If we lost control, all we can do now is close the files.
	 * In this case, the last close sets the process running.
	 */
	if (P->state != PS_STOP &&
	    (P->status.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP)) == 0) {
		Pfree(P);
		return;
	}

	/*
	 * We didn't lose control; we do more.
	 */
	Psync(P);

	if (flags & PRELEASE_CLEAR)
		P->flags |= CREATED;

	if (!(flags & PRELEASE_RETAIN))
		restore_tracing_flags(P);

	if (flags & PRELEASE_HANG) {
		/* Leave the process stopped and abandoned */
		(void) Punsetflags(P, PR_RLC|PR_KLC);
		Pfree(P);
		return;
	}

	/*
	 * Set the process running if we created it or if it was
	 * not originally stopped or directed to stop via /proc
	 * or if we were given the PRELEASE_CLEAR flag.
	 */
	if ((P->flags & CREATED) ||
	    (P->orig_status.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP)) == 0) {
		/*
		 * We do this repeatedly because the process may have
		 * more than one lwp stopped on an event of interest.
		 * This makes sure all of them are set running.
		 */
		do {
			(void) Psetrun(P, 0, 0);
		} while (Pstopstatus(P, PCNULL, 0) == 0 &&
		    P->status.pr_lwp.pr_flags & (PR_ISTOP|PR_DSTOP));
	}

	Pfree(P);
}

/* debugging */
static void
prdump(struct ps_prochandle *P)
{
	char name[32];
	uint32_t bits;

	switch (P->status.pr_lwp.pr_why) {
	case PR_REQUESTED:
		(void) fprintf(stderr, "Pstopstatus: REQUESTED");
		break;
	case PR_SIGNALLED:
		(void) fprintf(stderr, "Pstopstatus: SIGNALLED %s",
			proc_signame(P->status.pr_lwp.pr_what,
			name, sizeof (name)));
		break;
	case PR_FAULTED:
		(void) fprintf(stderr, "Pstopstatus: FAULTED %s",
			proc_fltname(P->status.pr_lwp.pr_what,
			name, sizeof (name)));
		break;
	case PR_SYSENTRY:
		(void) fprintf(stderr, "Pstopstatus: SYSENTRY %s",
			proc_sysname(P->status.pr_lwp.pr_what,
			name, sizeof (name)));
		break;
	case PR_SYSEXIT:
		(void) fprintf(stderr, "Pstopstatus: SYSEXIT %s",
			proc_sysname(P->status.pr_lwp.pr_what,
			name, sizeof (name)));
		break;
	case PR_JOBCONTROL:
		(void) fprintf(stderr, "Pstopstatus: JOBCONTROL %s",
			proc_signame(P->status.pr_lwp.pr_what,
			name, sizeof (name)));
		break;
	case PR_SUSPENDED:
		(void) fprintf(stderr, "Pstopstatus: SUSPENDED");
		break;
	default:
		(void) fprintf(stderr, "Pstopstatus: Unknown");
		break;
	}

	if (P->status.pr_lwp.pr_cursig)
		(void) fprintf(stderr, "  p_cursig  = %d",
			P->status.pr_lwp.pr_cursig);
	bits = *((uint32_t *)&P->status.pr_sigpend);
	if (bits)
		(void) fprintf(stderr, "  pr_sigpend = 0x%.8X", bits);
	bits = *((uint32_t *)&P->status.pr_lwp.pr_lwppend);
	if (bits)
		(void) fprintf(stderr, "  pr_lwppend = 0x%.8X", bits);
	(void) fputc('\n', stderr);
}

/*
 * Wait for the process to stop for any reason.
 */
int
Pwait(struct ps_prochandle *P, u_int msec)
{
	return (Pstopstatus(P, PCWSTOP, msec));
}

/*
 * Direct the process to stop; wait for it to stop.
 */
int
Pstop(struct ps_prochandle *P, u_int msec)
{
	return (Pstopstatus(P, PCSTOP, msec));
}

/*
 * Wait for the specified process to stop or terminate.
 * Or, just get the current status (PCNULL).
 * Or, direct it to stop and get the current status (PCDSTOP).
 * If the agent lwp exists, do these things to the agent,
 * else do these things to the process as a whole.
 */
static int
Pstopstatus(struct ps_prochandle *P,
	long request,		/* PCNULL, PCDSTOP, PCSTOP, PCWSTOP */
	u_int msec)		/* if non-zero, timeout in milliseconds */
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl[3];
	ssize_t rc;
	int err;

	switch (P->state) {
	case PS_RUN:
		break;
	case PS_STOP:
		if (request != PCNULL && request != PCDSTOP)
			return (0);
		break;
	case PS_LOST:
	case PS_DEAD:
		if (request != PCNULL)
			return (-1);
		break;
	default:	/* corrupted state */
		(void) fprintf(stderr, "Pstopstatus: corrupted state: %d\n",
			P->state);
		return (-1);
	}

	ctl[0] = PCDSTOP;
	ctl[1] = PCTWSTOP;
	ctl[2] = (long)msec;
	rc = 0;
	switch (request) {
	case PCSTOP:
		rc = write(ctlfd, &ctl[0], 3*sizeof (long));
		break;
	case PCWSTOP:
		rc = write(ctlfd, &ctl[1], 2*sizeof (long));
		break;
	case PCDSTOP:
		rc = write(ctlfd, &ctl[0], 1*sizeof (long));
		break;
	case PCNULL:
		break;
	default:	/* programming error */
		(void) fprintf(stderr, "Pstopstatus: illegal request\n");
		return (-1);
	}
	err = (rc < 0)? errno : 0;
	Psync(P);

	if (P->agentstatfd < 0) {
		if (pread(P->statfd, &P->status,
		    sizeof (P->status), (off_t)0) < 0)
			err = errno;
	} else {
		if (pread(P->agentstatfd, &P->status.pr_lwp,
		    sizeof (P->status.pr_lwp), (off_t)0) < 0)
			err = errno;
		P->status.pr_flags = P->status.pr_lwp.pr_flags;
	}

	if (err) {
		switch (err) {
		case EINTR:		/* user typed ctl-C */
		case ERESTART:
			if (_libproc_debug)
				(void) fprintf(stderr, "Pstopstatus: EINTR\n");
			break;
		case EAGAIN:		/* we lost control of the the process */
			if (_libproc_debug)
				(void) fprintf(stderr, "Pstopstatus: EAGAIN\n");
			P->state = PS_LOST;
			break;
		default:		/* check for dead process */
			if (_libproc_debug) {
				const char *errstr;

				switch (request) {
				case PCNULL:
					errstr = "Pstopstatus PCNULL"; break;
				case PCSTOP:
					errstr = "Pstopstatus PCSTOP"; break;
				case PCDSTOP:
					errstr = "Pstopstatus PCDSTOP"; break;
				case PCWSTOP:
					errstr = "Pstopstatus PCWSTOP"; break;
				default:
					errstr = "Pstopstatus PC???"; break;
				}
				perror(errstr);
			}
			deadcheck(P);
			break;
		}
		if (err != EINTR && err != ERESTART) {
			errno = err;
			return (-1);
		}
	}

	if (!(P->status.pr_flags & PR_STOPPED)) {
		P->state = PS_RUN;
		if (request == PCNULL || request == PCDSTOP || msec != 0)
			return (0);
		if (_libproc_debug)
			(void) fprintf(stderr,
				"Pstopstatus: process is not stopped\n");
		return (-1);
	}

	P->state = PS_STOP;

	if (_libproc_debug)	/* debugging */
		prdump(P);

	switch (P->status.pr_lwp.pr_why) {
	case PR_SYSENTRY:
#ifdef sparc
		/*
		 * The sparc syscall trap leaves the %pc unchanged.
		 * All others bump the %pc to the next instruction.
		 */
		P->sysaddr = P->REG[R_PC];
#else
		P->sysaddr = P->REG[R_PC] - sizeof (syscall_t);
#endif
		break;
	case PR_SYSEXIT:
		P->sysaddr = P->REG[R_PC] - sizeof (syscall_t);
		break;
	case PR_REQUESTED:
	case PR_SIGNALLED:
	case PR_FAULTED:
	case PR_JOBCONTROL:
	case PR_SUSPENDED:
		break;
	default:
		return (-1);
	}

	return (0);
}

static void
deadcheck(struct ps_prochandle *P)
{
	int fd;
	char *buf;
	size_t size;

	if (P->statfd < 0)
		P->state = PS_DEAD;
	else {
		if (P->agentstatfd < 0) {
			fd = P->statfd;
			buf = (char *)&P->status;
			size = sizeof (P->status);
		} else {
			fd = P->agentstatfd;
			buf = (char *)&P->status.pr_lwp;
			size = sizeof (P->status.pr_lwp);
		}
		while (pread(fd, buf, size, (off_t)0) != size) {
			switch (errno) {
			default:
				/* process or lwp is dead */
				P->state = PS_DEAD;
				break;
			case EINTR:
			case ERESTART:
				continue;
			case EAGAIN:
				P->state = PS_LOST;
				break;
			}
			break;
		}
		P->status.pr_flags = P->status.pr_lwp.pr_flags;
	}
}

/*
 * Get the value of one register from stopped process.
 */
int
Pgetareg(struct ps_prochandle *P, int regno, prgreg_t *preg)
{
	if (regno < 0 || regno >= NPRGREG) {
		(void) fprintf(stderr,
			"Pgetareg(): invalid register number, %d\n", regno);
		return (-1);
	}
	if (P->state != PS_STOP)
		return (-1);

	*preg = P->status.pr_lwp.pr_reg[regno];
	return (0);
}

/*
 * Put value of one register into stopped process.
 */
int
Pputareg(struct ps_prochandle *P, int regno, prgreg_t reg)
{
	if (regno < 0 || regno >= NPRGREG) {
		(void) fprintf(stderr,
			"Pputareg(): invalid register number, %d\n", regno);
		return (-1);
	}
	if (P->state != PS_STOP)
		return (-1);

	P->status.pr_lwp.pr_reg[regno] = reg;
	P->flags |= SETREGS;	/* set registers before continuing */
	return (0);
}

int
Psetrun(struct ps_prochandle *P,
	int sig,	/* signal to pass to process */
	int flags)	/* PRSTEP|PRSABORT|PRSTOP|PRCSIG|PRCFAULT */
{
	int ctlfd;
	long ctl[1 +					/* PCCFAULT	*/
		1 + sizeof (siginfo_t)/sizeof (long) +	/* PCSSIG/PCCSIG */
		2 ];					/* PCRUN	*/
	long *ctlp = ctl;
	size_t size;

	if (P->state != PS_STOP && !(P->status.pr_lwp.pr_flags & PR_DSTOP))
		return (-1);

	Psync(P);	/* flush tracing flags and registers */

	if (flags & PRCFAULT) {		/* clear current fault */
		*ctlp++ = PCCFAULT;
		flags &= ~PRCFAULT;
	}

	if (flags & PRCSIG) {		/* clear current signal */
		*ctlp++ = PCCSIG;
		flags &= ~PRCSIG;
	} else if (sig && sig != P->status.pr_lwp.pr_cursig) {
		/* make current signal */
		siginfo_t *infop;

		*ctlp++ = PCSSIG;
		infop = (siginfo_t *)ctlp;
		(void) memset(infop, 0, sizeof (*infop));
		infop->si_signo = sig;
		ctlp += sizeof (siginfo_t) / sizeof (long);
	}

	*ctlp++ = PCRUN;
	*ctlp++ = flags;
	size = (char *)ctlp - (char *)ctl;

	P->info_valid = 0;	/* will need to update map and file info */

	ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	if (write(ctlfd, ctl, size) != size) {
		/* If it is dead or lost, return the real status, not PS_RUN */
		if (errno == ENOENT || errno == EAGAIN) {
			(void) Pstopstatus(P, PCNULL, 0);
			return (0);
		}
		/* If it is not in a jobcontrol stop, issue an error message */
		if (errno != EBUSY ||
		    P->status.pr_lwp.pr_why != PR_JOBCONTROL) {
			perror("Psetrun");
			return (-1);
		}
		/* Otherwise pretend that the job-stopped process is running */
	}

	P->state = PS_RUN;
	return (0);
}

ssize_t
Pread(struct ps_prochandle *P,
	void *buf,		/* caller's buffer */
	size_t nbyte,		/* number of bytes to read */
	uintptr_t address)	/* address in process */
{
	return (pread(P->asfd, buf, nbyte, (off_t)address));
}

ssize_t
Pwrite(struct ps_prochandle *P,
	const void *buf,	/* caller's buffer */
	size_t nbyte,		/* number of bytes to write */
	uintptr_t address)	/* address in process */
{
	return (pwrite(P->asfd, buf, nbyte, (off_t)address));
}

int
Pclearsig(struct ps_prochandle *P)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl = PCCSIG;

	if (write(ctlfd, &ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);
	P->status.pr_lwp.pr_cursig = 0;
	return (0);
}

int
Pclearfault(struct ps_prochandle *P)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	long ctl = PCCFAULT;

	if (write(ctlfd, &ctl, sizeof (ctl)) != sizeof (ctl))
		return (-1);
	return (0);
}

#if defined(sparc) || defined(__sparc)
#define	BPT	((instr_t)0x91d02001)
#elif defined(i386) || defined(__i386)
#define	BPT	((instr_t)0xcc)
#endif
/*
 * Set a breakpoint trap, return original instruction.
 */
int
Psetbkpt(struct ps_prochandle *P, uintptr_t address, u_long *saved)
{
	long ctl[1 + sizeof (priovec_t) / sizeof (long) +	/* PCREAD */
		1 + sizeof (priovec_t) / sizeof (long)];	/* PCWRITE */
	long *ctlp = ctl;
	size_t size;
	priovec_t *iovp;
	instr_t bpt = BPT;
	instr_t old;

	/* fetch the old instruction */
	*ctlp++ = PCREAD;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &old;
	iovp->pio_len = sizeof (old);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	/* write the BPT instruction */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &bpt;
	iovp->pio_len = sizeof (bpt);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	size = (char *)ctlp - (char *)ctl;
	if (write(P->ctlfd, ctl, size) != size)
		return (-1);

	*saved = (u_long)old;
	return (0);
}

/*
 * Restore original instruction where a breakpoint was set.
 */
int
Pdelbkpt(struct ps_prochandle *P, uintptr_t address, u_long saved)
{
	instr_t old = (instr_t)saved;

	if (Pwrite(P, &old, sizeof (old), address) != sizeof (old))
		return (-1);
	return (0);
}

/*
 * Step over a breakpoint, i.e., execute the instruction that
 * really belongs at the breakpoint location (the current %pc)
 * and leave the process stopped at the next instruction.
 */
int
Pxecbkpt(struct ps_prochandle *P, u_long saved)
{
	uintptr_t address = P->REG[R_PC];
	long ctl[1 + sizeof (priovec_t) / sizeof (long) +	/* PCWRITE */
		1 +						/* PCCFAULT */
		1 + 						/* PCCSIG */
		1 + sizeof (sigset_t) / sizeof (long) +		/* PCSHOLD */
		1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
		2 +						/* PCRUN */
		1 +						/* PCWSTOP */
		1 +						/* PCCFAULT */
		1 + sizeof (fltset_t) / sizeof (long) +		/* PCSFAULT */
		1 + sizeof (sigset_t) / sizeof (long) +		/* PCSHOLD */
		1 + sizeof (priovec_t) / sizeof (long)];	/* PCWRITE */
	long *ctlp = ctl;
	sigset_t block, unblock;
	size_t size;
	priovec_t *iovp;
	sigset_t *holdp;
	fltset_t *faultp;
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	instr_t old = (instr_t)saved;
	instr_t bpt = BPT;
	int rv;

	if (P->state != PS_STOP)
		return (-1);
	(void) Psync(P);

	/* block our signals for the duration */
	(void) sigfillset(&block);
	(void) sigemptyset(&unblock);
	(void) sigprocmask(SIG_BLOCK, &block, &unblock);

	/* restore the old instruction */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &old;
	iovp->pio_len = sizeof (old);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	*ctlp++ = PCCFAULT;
	*ctlp++ = PCCSIG;

	/* hold posted signals */
	*ctlp++ = PCSHOLD;
	holdp = (sigset_t *)ctlp;
	prfillset(holdp);
	prdelset(holdp, SIGKILL);
	prdelset(holdp, SIGSTOP);
	ctlp += sizeof (sigset_t) / sizeof (long);

	/* force tracing of FLTTRACE */
	if (!(prismember(&P->status.pr_flttrace, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		faultp = (fltset_t *)ctlp;
		*faultp = P->status.pr_flttrace;
		praddset(faultp, FLTTRACE);
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/* set running w/ single-step */
	*ctlp++ = PCRUN;
	*ctlp++ = PRSTEP;

	/* wait for stop, cancel the fault */
	*ctlp++ = PCWSTOP;
	*ctlp++ = PCCFAULT;

	/* restore fault tracing set */
	if (!(prismember(&P->status.pr_flttrace, FLTTRACE))) {
		*ctlp++ = PCSFAULT;
		faultp = (fltset_t *)ctlp;
		*faultp = P->status.pr_flttrace;
		ctlp += sizeof (fltset_t) / sizeof (long);
	}

	/* restore the hold mask */
	*ctlp++ = PCSHOLD;
	*(sigset_t *)ctlp = P->status.pr_lwp.pr_lwphold;
	ctlp += sizeof (sigset_t) / sizeof (long);

	/* restore the breakpoint trap */
	*ctlp++ = PCWRITE;
	iovp = (priovec_t *)ctlp;
	iovp->pio_base = &bpt;
	iovp->pio_len = sizeof (bpt);
	iovp->pio_offset = address;
	ctlp += sizeof (priovec_t) / sizeof (long);

	P->state = PS_RUN;
	size = (char *)ctlp - (char *)ctl;
	if (write(ctlfd, ctl, size) != size) {
		(void) Pstopstatus(P, PCNULL, 0);
		/* unblock our signals before returning */
		(void) sigprocmask(SIG_SETMASK, &unblock, NULL);
		if (P->status.pr_lwp.pr_why == PR_JOBCONTROL &&
		    errno == EBUSY) {
			/* jobcontrol stop -- back off */
			P->state = PS_RUN;
			return (0);
		}
		if (errno == ENOENT)
			return (0);
		perror("Pxecbkpt");
		return (-1);
	}
	rv = Pstopstatus(P, PCNULL, 0);
	/* unblock our signals before returning */
	(void) sigprocmask(SIG_SETMASK, &unblock, NULL);
	return (rv);
}

int
Psetflags(struct ps_prochandle *P, long flags)
{
	int rc;
	long ctl[2];

	ctl[0] = PCSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		rc = -1;
	} else {
		P->status.pr_flags |= flags;
		P->status.pr_lwp.pr_flags |= flags;
		rc = 0;
	}

	return (rc);
}

int
Punsetflags(struct ps_prochandle *P, long flags)
{
	int rc;
	long ctl[2];

	ctl[0] = PCUNSET;
	ctl[1] = flags;

	if (write(P->ctlfd, ctl, 2*sizeof (long)) != 2*sizeof (long)) {
		rc = -1;
	} else {
		P->status.pr_flags &= ~flags;
		P->status.pr_lwp.pr_flags &= ~flags;
		rc = 0;
	}

	return (rc);
}

/*
 * Set action on specified signal.
 * If 'which' is zero, set the action for all signals.
 */
int
Psignal(struct ps_prochandle *P,
	int which,		/* signal number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	int oldval;

	if (which < 0 || which > PRMAXSIG || (which == SIGKILL && stop))
		return (-1);

	oldval = prismember(&P->status.pr_sigtrace, which)? TRUE : FALSE;

	if (stop) {	/* stop process on receipt of signal */
		if (which == 0) {
			prfillset(&P->status.pr_sigtrace);
			prdelset(&P->status.pr_sigtrace, SIGKILL);
			P->flags |= SETSIG;
		} else if (!oldval) {
			praddset(&P->status.pr_sigtrace, which);
			P->flags |= SETSIG;
		}
	} else {	/* let process continue on receipt of signal */
		if (which == 0) {
			premptyset(&P->status.pr_sigtrace);
			P->flags |= SETSIG;
		} else if (oldval) {
			prdelset(&P->status.pr_sigtrace, which);
			P->flags |= SETSIG;
		}
	}

	return (oldval);
}

/*
 * Set all signal tracing flags.
 */
void
Psetsignal(struct ps_prochandle *P, const sigset_t *set)
{
	P->status.pr_sigtrace = *set;
	P->flags |= SETSIG;
}

/*
 * Set action on specified fault.
 * If 'which' is zero, set the action for all faults.
 */
int
Pfault(struct ps_prochandle *P,
	int which,		/* fault number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	int oldval;

	if (which < 0 || which > PRMAXFAULT)
		return (-1);

	oldval = prismember(&P->status.pr_flttrace, which)? TRUE : FALSE;

	if (stop) {	/* stop process on receipt of fault */
		if (which == 0) {
			prfillset(&P->status.pr_flttrace);
			P->flags |= SETFAULT;
		} else if (!oldval) {
			praddset(&P->status.pr_flttrace, which);
			P->flags |= SETFAULT;
		}
	} else {	/* let process continue on receipt of fault */
		if (which == 0) {
			premptyset(&P->status.pr_flttrace);
			P->flags |= SETFAULT;
		} else if (oldval) {
			prdelset(&P->status.pr_flttrace, which);
			P->flags |= SETFAULT;
		}
	}

	return (oldval);
}

/*
 * Set all machine fault tracing flags.
 */
void
Psetfault(struct ps_prochandle *P, const fltset_t *set)
{
	P->status.pr_flttrace = *set;
	P->flags |= SETFAULT;
}

/*
 * Set action on specified system call entry.
 * If 'which' is zero, set the action for entry to all syscalls.
 */
int
Psysentry(struct ps_prochandle *P,
	int which,		/* system call number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	int oldval;

	if (which < 0 || which > PRMAXSYS)
		return (-1);

	oldval = prismember(&P->status.pr_sysentry, which)? TRUE : FALSE;

	if (stop) {	/* stop process on sys call */
		if (which == 0) {
			prfillset(&P->status.pr_sysentry);
			P->flags |= SETENTRY;
		} else if (!oldval) {
			praddset(&P->status.pr_sysentry, which);
			P->flags |= SETENTRY;
		}
	} else {	/* don't stop process on sys call */
		if (which == 0) {
			premptyset(&P->status.pr_sysentry);
			P->flags |= SETENTRY;
		} else if (oldval) {
			prdelset(&P->status.pr_sysentry, which);
			P->flags |= SETENTRY;
		}
	}

	return (oldval);
}

/*
 * Set all system call entry tracing flags.
 */
void
Psetsysentry(struct ps_prochandle *P, const sysset_t *set)
{
	P->status.pr_sysentry = *set;
	P->flags |= SETENTRY;
}

/*
 * Set action on specified system call exit.
 * If 'which' is zero, set the action for exit from all syscalls.
 */
int
Psysexit(struct ps_prochandle *P,
	int which,		/* system call number */
	int stop)		/* if TRUE, stop process; else let it go */
{
	int oldval;

	if (which < 0 || which > PRMAXSYS)
		return (-1);

	oldval = prismember(&P->status.pr_sysexit, which)? TRUE : FALSE;

	if (stop) {	/* stop process on sys call exit */
		if (which == 0) {
			prfillset(&P->status.pr_sysexit);
			P->flags |= SETEXIT;
		} else if (!oldval) {
			praddset(&P->status.pr_sysexit, which);
			P->flags |= SETEXIT;
		}
	} else {	/* don't stop process on sys call exit */
		if (which == 0) {
			premptyset(&P->status.pr_sysexit);
			P->flags |= SETEXIT;
		} else if (oldval) {
			prdelset(&P->status.pr_sysexit, which);
			P->flags |= SETEXIT;
		}
	}

	return (oldval);
}

/*
 * Set all system call exit tracing flags.
 */
void
Psetsysexit(struct ps_prochandle *P, const sysset_t *set)
{
	P->status.pr_sysexit = *set;
	P->flags |= SETEXIT;
}

/*
 * lwp iteration interface.
 */
int
proc_lwp_iter(struct ps_prochandle *P, proc_lwp_f *func, void *cd)
{
	prheader_t *Lhp;
	lwpstatus_t *Lsp;
	char lstatus[64];
	struct stat statb;
	int fd;
	long nlwp;
	int rv = -1;

	if (P->state != PS_STOP)
		return (-1);
	(void) Psync(P);

	/* single-lwp is easy */
	if (P->status.pr_nlwp <= 1)
		return (func(cd, (const lwpstatus_t *)&P->status.pr_lwp));

	/* multiple-lwp is a bit harder */
	(void) sprintf(lstatus, "/proc/%d/lstatus", (int)P->status.pr_pid);
	if ((fd = open(lstatus, O_RDONLY)) < 0 || fstat(fd, &statb) != 0) {
		if (fd >= 0)
			(void) close(fd);
		return (-1);
	}

	Lhp = malloc(statb.st_size);
	if (read(fd, Lhp, statb.st_size) <
	    sizeof (prheader_t) + sizeof (lwpstatus_t)) {
		(void) close(fd);
		free(Lhp);
		return (-1);
	}
	(void) close(fd);

	/* LINTED improper alignment */
	for (nlwp = Lhp->pr_nent, Lsp = (lwpstatus_t *)(Lhp + 1); nlwp > 0;
	    /* LINTED improper alignment */
	    nlwp--, Lsp = (lwpstatus_t *)((char *)Lsp + Lhp->pr_entsize)) {
		if ((rv = func(cd, (const lwpstatus_t *)Lsp)) != 0)
			break;
	}

	free(Lhp);
	return (rv);
}

/*
 * Execute the syscall instruction.
 */
static int
execute(struct ps_prochandle *P, int sysindex)
{
	int ctlfd = (P->agentctlfd >= 0)? P->agentctlfd : P->ctlfd;
	int washeld = FALSE;
	sigset_t hold;		/* mask of held signals */
	int cursig;
	struct {
		long cmd;
		siginfo_t siginfo;
	} ctl;
	int sentry;		/* old value of stop-on-syscall-entry */

	sentry = Psysentry(P, sysindex, TRUE);	/* set stop-on-syscall-entry */

	/*
	 * If not already blocked, block all signals now.
	 */
	if (memcmp(&P->status.pr_lwp.pr_lwphold, blockable_signals(),
	    sizeof (sigset_t)) != 0) {
		hold = P->status.pr_lwp.pr_lwphold;
		P->status.pr_lwp.pr_lwphold = *blockable_signals();
		P->flags |= SETHOLD;
		washeld = TRUE;
	}

	/*
	 * If there is a current signal, remember it and cancel it.
	 */
	if ((cursig = P->status.pr_lwp.pr_cursig) != 0) {
		ctl.cmd = PCSSIG;
		ctl.siginfo = P->status.pr_lwp.pr_info;
	}

	if (Psetrun(P, 0, PRCSIG) == -1)
		goto bad;

	while (P->state == PS_RUN)
		(void) Pwait(P, 0);
	if (P->state != PS_STOP)
		goto bad;

	if (cursig)				/* restore cursig */
		(void) write(ctlfd, &ctl, sizeof (ctl));
	if (washeld) {		/* restore the signal mask if we set it */
		P->status.pr_lwp.pr_lwphold = hold;
		P->flags |= SETHOLD;
	}
	(void) Psysentry(P, sysindex, sentry);	/* restore sysentry stop */

	if (P->status.pr_lwp.pr_why  == PR_SYSENTRY &&
	    P->status.pr_lwp.pr_what == sysindex)
		return (0);
bad:
	return (-1);
}


/*
 * Worst-case alignment for objects on the stack.
 */
#if i386	/* stack grows down, non-aligned */
#define	ALIGN32(sp)	(sp)
#define	ALIGN64(sp)	ALIGN32(sp)
#define	ARGOFF	1
#endif
#if sparc	/* stack grows down, doubleword-aligned */
#define	ALIGN32(sp)	((sp) & ~(2 * sizeof (int32_t) - 1))
#define	ALIGN64(sp)	((sp) & ~(2 * sizeof (int64_t) - 1))
#define	ARGOFF	0
#ifndef	WINDOWSIZE32
#define	WINDOWSIZE32	(16 * sizeof (int32_t))
#endif
#ifndef	WINDOWSIZE64
#define	WINDOWSIZE64	(16 * sizeof (int64_t))
#endif
#endif

/*
 * Perform system call in controlled process.
 */
sysret_t
Psyscall(struct ps_prochandle *P,
	int sysindex,		/* system call index */
	u_int nargs,		/* number of arguments to system call */
	argdes_t *argp)		/* argument descriptor array */
{
	int agent_created = FALSE;
	pstatus_t save_pstatus;
	argdes_t *adp;			/* pointer to argument descriptor */
	sysret_t rval;			/* return value */
	int i;				/* general index value */
	int model;			/* data model */
	int Perr = 0;			/* local error number */
	int sexit;			/* old value of stop-on-syscall-exit */
	prgreg_t sp;			/* adjusted stack pointer */
	prgreg_t ap;			/* adjusted argument pointer */
	int32_t arglist32[MAXARGS+2];	/* 32-bit syscall arglist */
	int64_t arglist64[MAXARGS+2];	/* 64-bit syscall arglist */
	sigset_t block, unblock;

	/* block (hold) all signals for the duration. */
	(void) sigfillset(&block);
	(void) sigemptyset(&unblock);
	(void) sigprocmask(SIG_BLOCK, &block, &unblock);

	rval.sys_errno = 0;		/* initialize return value */
	rval.sys_rval1 = 0;
	rval.sys_rval2 = 0;

	if (sysindex <= 0 || sysindex > PRMAXSYS || nargs > MAXARGS)
		goto bad1;	/* programming error */

	model = P->status.pr_dmodel;
#ifndef _LP64
	/* We must be a 64-bit process to deal with a 64-bit process */
	if (model == PR_MODEL_LP64)
		goto bad9;
#endif

	/*
	 * Create the /proc agent lwp in the process to do all the work.
	 * (It may already exist; nested create/destroy is permitted
	 * by virtue of the reference count.)
	 */
	if (Pcreate_agent(P) != 0)
		goto bad8;

	/*
	 * Save agent's status to restore on exit.
	 */
	agent_created = TRUE;
	save_pstatus = P->status;

	if (P->state != PS_STOP ||		/* check state of lwp */
	    (P->status.pr_flags & PR_ASLEEP))
		goto bad2;

	if (checksyscall(P))			/* bad text ? */
		goto bad3;

	/*
	 * Validate arguments and compute the stack frame parameters.
	 * Begin with the current stack pointer.
	 */
	if (model == PR_MODEL_LP64) {
		sp = P->REG[R_SP] + STACK_BIAS;
		sp = ALIGN64(sp);
	} else {
		sp = (uint32_t)P->REG[R_SP];
		sp = ALIGN32(sp);
	}

	/*
	 * For each AT_BYREF argument, compute the necessary
	 * stack space and the object's stack address.
	 */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.sys_rval1 = i;		/* in case of error */
		switch (adp->arg_type) {
		default:			/* programming error */
			goto bad4;
		case AT_BYVAL:			/* simple argument */
			break;
		case AT_BYREF:			/* must allocate space */
			switch (adp->arg_inout) {
			case AI_INPUT:
			case AI_OUTPUT:
			case AI_INOUT:
				if (adp->arg_object == NULL)
					goto bad5;	/* programming error */
				break;
			default:		/* programming error */
				goto bad6;
			}
			/* allocate stack space for BYREF argument */
			if (adp->arg_size == 0 || adp->arg_size > MAXARGL)
				goto bad7;	/* programming error */
#ifdef _STACK_GROWS_DOWNWARD
			if (model == PR_MODEL_LP64)
				sp = ALIGN64(sp - adp->arg_size);
			else
				sp = ALIGN32(sp - adp->arg_size);
			adp->arg_value = sp;	/* stack address for object */
#else				/* upward stack growth */
			adp->arg_value = sp;	/* stack address for object */
			if (model == PR_MODEL_LP64)
				sp = ALIGN64(sp + adp->arg_size);
			else
				sp = ALIGN32(sp + adp->arg_size);
#endif
			break;
		}
	}
	rval.sys_rval1 = 0;			/* in case of error */
#if i386
	sp -= sizeof (int) * (nargs+2);	/* space for arg list + CALL parms */
	ap = sp;			/* address of arg list */
#endif
#if sparc
	if (model == PR_MODEL_LP64) {
		sp -= (nargs > 6)?
			WINDOWSIZE64 + sizeof (int64_t) * nargs :
			WINDOWSIZE64 + sizeof (int64_t) * 6;
		sp = ALIGN64(sp);
		/* ap == address of arg dump area */
		ap = sp + WINDOWSIZE64;
	} else {
		sp -= (nargs > 6)?
			WINDOWSIZE32 + sizeof (int32_t) * (1 + nargs) :
			WINDOWSIZE32 + sizeof (int32_t) * (1 + 6);
		sp = ALIGN32(sp);
		/* ap == address of arg dump area */
		ap = sp + WINDOWSIZE32 + sizeof (int32_t);
	}
#endif

	/*
	 * Point of no return.
	 * Perform the system call entry, adjusting %sp.
	 * This moves the lwp to the stopped-on-syscall-entry state
	 * just before the arguments to the system call are fetched.
	 */
#if sparc
	P->REG[R_G1] = sysindex;
#elif i386
	P->REG[EAX] = sysindex;
#endif
	if (model == PR_MODEL_LP64)
		P->REG[R_SP] = sp - STACK_BIAS;
	else
		P->REG[R_SP] = sp;
	P->REG[R_PC] = P->sysaddr;	/* address of syscall */
#if sparc
	P->REG[R_nPC] = P->sysaddr + sizeof (syscall_t);
#endif
	P->flags |= SETREGS;	/* set registers before continuing */

	if (_libproc_debug)
		(void) fprintf(stderr, "Psyscall(): execute(sysindex = %d)\n",
			sysindex);

	/*
	 * Execute the syscall instruction and stop on syscall entry.
	 */
#if sparc
	if (execute(P, sysindex) != 0 ||
	    P->REG[R_PC] != P->sysaddr ||
	    P->REG[R_nPC] != P->sysaddr + sizeof (syscall_t))
#else
	if (execute(P, sysindex) != 0 ||
	    P->REG[R_PC] != P->sysaddr + sizeof (syscall_t))
#endif
		goto bad10;

	if (_libproc_debug)
		(void) fprintf(stderr, "Psyscall(): copying arguments\n");

	/*
	 * The lwp is stopped at syscall entry.
	 * Copy objects to stack frame for each argument.
	 */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.sys_rval1 = i;		/* in case of error */
		if (adp->arg_type != AT_BYVAL &&
		    adp->arg_inout != AI_OUTPUT) {
			/* copy input byref parameter to process */
			if (Pwrite(P, adp->arg_object, adp->arg_size,
			    (uintptr_t)adp->arg_value) != adp->arg_size)
				goto bad17;
		}
		arglist32[ARGOFF+i] = (int32_t)adp->arg_value;
		arglist64[ARGOFF+i] = (int64_t)adp->arg_value;
#if sparc
		if (i < 6)
			(void) Pputareg(P, R_O0+i, adp->arg_value);
#endif
	}
	rval.sys_rval1 = 0;			/* in case of error */
#if i386
	arglist32[0] = P->REG[R_PC];		/* CALL parameters */
	if (Pwrite(P, &arglist32[0], sizeof (int) * (nargs+1),
	    (uintptr_t)ap) != sizeof (int) * (nargs+1))
		goto bad18;
#endif
#if sparc
	if (model == PR_MODEL_LP64) {
		if (nargs > 6 &&
		    Pwrite(P, &arglist64[0], sizeof (int64_t) * nargs,
		    (uintptr_t)ap) != sizeof (int64_t) * nargs)
			goto bad18;
	} else {
		if (nargs > 6 &&
		    Pwrite(P, &arglist32[0], sizeof (int32_t) * nargs,
		    (uintptr_t)ap) != sizeof (int32_t) * nargs)
			goto bad18;
	}
#endif

	/*
	 * Complete the system call.
	 * This moves the lwp to the stopped-on-syscall-exit state.
	 */
	if (_libproc_debug)
		(void) fprintf(stderr, "Psyscall(): set running at sysentry\n");

	sexit = Psysexit(P, sysindex, TRUE);	/* catch this syscall exit */
	do {
		if (Psetrun(P, 0, 0) == -1)
			goto bad21;
		while (P->state == PS_RUN)
			(void) Pwait(P, 0);
	} while (P->state == PS_STOP && P->status.pr_lwp.pr_why != PR_SYSEXIT);
	(void) Psysexit(P, sysindex, sexit);	/* restore original setting */

	if (P->state != PS_STOP ||
	    P->status.pr_lwp.pr_why  != PR_SYSEXIT)
		goto bad22;
	if (P->status.pr_lwp.pr_what != sysindex)
		goto bad23;
#if sparc
	if (P->REG[R_PC] != P->sysaddr + sizeof (syscall_t) ||
	    P->REG[R_nPC] != P->sysaddr + 2*sizeof (syscall_t))
#else
	if (P->REG[R_PC] != P->sysaddr + sizeof (syscall_t))
#endif
		goto bad24;

	if (_libproc_debug)
		(void) fprintf(stderr, "Psyscall(): caught at sysexit\n");

	/*
	 * Fetch output arguments back from the lwp.
	 */
#if i386
	if (Pread(P, &arglist32[0], sizeof (int) * (nargs+1), (uintptr_t)ap)
	    != sizeof (int) * (nargs+1))
		goto bad25;
#endif
	/*
	 * For each argument.
	 */
	for (i = 0, adp = argp; i < nargs; i++, adp++) {
		rval.sys_rval1 = i;		/* in case of error */
		if (adp->arg_type != AT_BYVAL &&
		    adp->arg_inout != AI_INPUT) {
			/* copy output byref parameter from process */
			if (Pread(P, adp->arg_object, adp->arg_size,
			    (uintptr_t)adp->arg_value) != adp->arg_size)
				goto bad26;
		}
		if (model == PR_MODEL_LP64)
			adp->arg_value = arglist64[ARGOFF+i];
		else
			adp->arg_value = arglist32[ARGOFF+i];
	}


	/*
	 * Get the return values from the syscall.
	 */
	if (P->status.pr_lwp.pr_errno) {		/* error */
		rval.sys_errno = P->status.pr_lwp.pr_errno;
		rval.sys_rval1 = -1;
		if (_libproc_debug) {
			(void) fprintf(stderr,
				"Psyscall(%d) fails with errno %d\n",
				sysindex, rval.sys_errno);
		}
	} else {				/* normal return */
		rval.sys_rval1 = P->status.pr_lwp.pr_rval1;
		rval.sys_rval2 = P->status.pr_lwp.pr_rval2;
		if (_libproc_debug) {
			(void) fprintf(stderr,
				"Psyscall(%d) returns 0x%lx 0x%lx\n",
				sysindex,
				P->status.pr_lwp.pr_rval1,
				P->status.pr_lwp.pr_rval2);
		}
	}

	goto out;

bad26:	Perr++;
bad25:	Perr++;
bad24:	Perr++;
bad23:	Perr++;
bad22:	Perr++;
bad21:	Perr++;
	Perr++;
	Perr++;
bad18:	Perr++;
bad17:	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
	Perr++;
bad10:	Perr++;
bad9:	Perr++;
bad8:	Perr++;
bad7:	Perr++;
bad6:	Perr++;
bad5:	Perr++;
bad4:	Perr++;
bad3:	Perr++;
bad2:	Perr++;
bad1:	Perr++;
	rval.sys_errno = -Perr;	/* local errors are negative */
	if (_libproc_debug) {
		(void) fprintf(stderr,
			"Psyscall(%d) fails with local error %d\n",
			sysindex, Perr);
	}

out:
	/*
	 * Destroy the /proc agent lwp now (or just bump down the ref count).
	 */
	if (agent_created) {
		if (P->state != PS_DEAD) {
			P->status = save_pstatus;
			P->flags |= SETREGS;
			Psync(P);
		}
		Pdestroy_agent(P);
	}

	/*
	 * Unblock (release) all signals before returning.
	 */
	(void) sigprocmask(SIG_SETMASK, &unblock, (sigset_t *)NULL);

	return (rval);
}

/*
 * Check syscall instruction in process.
 */
static int
checksyscall(struct ps_prochandle *P)
{
	/* this should always succeed--we always have a good syscall address */
	syscall_t instr;		/* holds one syscall instruction */

	if (Pread(P, &instr, sizeof (instr), P->sysaddr)
	    == sizeof (instr)) {
#if i386
		if (instr[0] == SYSCALL)
#elif sparc
		if ((P->status.pr_dmodel == PR_MODEL_LP64)?
		    (instr == (syscall_t)SYSCALL64) :
		    (instr == (syscall_t)SYSCALL32))
#else
		if (instr == (syscall_t)SYSCALL)
#endif
			return (0);
	}

	/*
	 * Do it the hard way: search address space for a syscall instruction.
	 */
	if (Pscantext(P) == 0)
		return (0);

	return (-1);
}
