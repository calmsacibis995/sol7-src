/*	Copyright (c) 1993,1997 by Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sys.c	1.15	97/03/13 SMI"

#include <sys/types.h>
#include <stropts.h>
#include <poll.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/uio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <ucontext.h>


/*
 * Initialize libthread so that it knows pthreads are enabled.  This pragma
 * establishes a call to __pthread_init in a .init section.  Note that it is
 * not really necessary to define our own version of this function, however
 * the user of -zdefs makes it simpler if we do.
 */
#pragma init(__pthread_init)

void
__pthread_init()
{
}

_getfp() {
	return (0);
}

/*
 * Stub library for programmer's interface to the dynamic linker.  Used
 * to satisfy ld processing, and serves as a precedence place-holder at
 * execution-time.  These routines are never actually called.
 */

#ifdef DEBUG

#pragma weak _lwp_cond_wait = __lwp_cond_wait
#pragma weak _lwp_cond_timedwait = __lwp_cond_timedwait

/* ARGSUSED */
int
__lwp_cond_wait(cond_t *cv, mutex_t *mp)
{
	return (0);
}

/* ARGSUSED */
int
__lwp_cond_timedwait(cond_t *cv, mutex_t *mp, timestruc_t *ts)
{
	return (0);
}
#endif /* DEBUG */

#ifdef __STDC__

#pragma weak fork = _fork
#pragma weak fork1 = _fork1
#pragma weak sigaction = _sigaction
#pragma weak sigprocmask = _sigprocmask
#pragma weak sigwait = _sigwait
#pragma weak sigsuspend = _sigsuspend
#pragma weak sigsetjmp = _sigsetjmp
#pragma weak siglongjmp = _siglongjmp
#pragma weak sleep = _sleep
#pragma weak alarm = _alarm
#pragma weak setitimer = _setitimer

#endif /* __STDC__ */

/*
 * Following functions have been interposed in libthread,
 * weak as well as strong symbol
 */

int
_fork1()
{
	return (0);
}

int
_fork()
{
	return (0);
}

/* ARGSUSED */
_sigaction(int sig, const struct sigaction *nact, struct sigaction *oact)
{
	return (0);
}

/* ARGSUSED */
int
_sigprocmask(int how, sigset_t *set, sigset_t *oset)
{
	return (0);
}

/* ARGSUSED */
int
_sigwait(sigset_t *set)
{
	return (0);
}

/* ARGSUSED */
int
_sigsuspend(sigset_t *set)
{
	return (0);
}

/* ARGSUSED */
int
_sigsetjmp(sigjmp_buf env, int savemask)
{
	return (0);
}

/* ARGSUSED */
void
_siglongjmp(sigjmp_buf env, int val)
{

}

/* ARGSUSED */
unsigned
_sleep(unsigned sleep_tm)
{
	return (0);
}

/* ARGSUSED */
unsigned
_alarm(unsigned sec)
{
	return (0);
}

/* ARGSUSED */
int
_setitimer(int which, const struct itimerval *value,
			struct itimerval *ovalue)
{
	return (0);
}

/*
 * Following functions do not have pragma weak version.
 * These have been interposed as they are in libthread.
 */

/* ARGSUSED */
int
__sigtimedwait(const sigset_t *set, siginfo_t *info)
{
	return (0);
}

/*
 * Following functions do not have their "_" version in libthread.
 * XXX: Should libthread interpose on both weak and strong symbols?
 */

/* ARGSUSED */
int
setcontext(const ucontext_t *ucp)
{
	return (0);
}

/* ARGSUSED */
int
sigpending(sigset_t *set)
{
	return (0);
}

/*
 * Following C library entry points are defined here because they are
 * cancellation points and libthread interpose them.
 */

/* C library -- read						*/
/* ARGSUSED */
int
read(int fildes, void *buf, unsigned nbyte)
{
	return (-1);
}

/* C library -- close						*/
/* ARGSUSED */
int
close(int fildes)
{
	return (-1);
}

/* C library -- open						*/
/* ARGSUSED */
int
open(const char *path, int oflag, mode_t mode)
{
	return (-1);
}

/* C library -- write						*/
/* ARGSUSED */
int
write(int fildes, void *buf, unsigned nbyte)
{
	return (-1);
}

/* C library -- fcntl						*/
/* ARGSUSED */
int
fcntl(int fildes, int cmd, int arg)
{
	return (-1);
}

/* C library -- pause						*/
/* ARGSUSED */
int
pause(void)
{
	return (-1);
}


/* C library -- wait						*/
/* ARGSUSED */
int
wait(int *stat_loc)
{
	return (-1);
}

/* C library -- creat						*/
/* ARGSUSED */
int
creat(char *path, mode_t mode)
{
	return (-1);
}

/* C library -- fsync						*/
/* ARGSUSED */
int
fsync(int fildes, void *buf, unsigned nbyte)
{
	return (-1);
}


/* ARGSUSED */
msync(caddr_t addr, size_t  len, int flags)
{
	return (-1);
}

/* C library -- tcdrain						*/
/* ARGSUSED */
int
tcdrain(int fildes)
{
	return (-1);
}

/* C library -- waitpid						*/
/* ARGSUSED */
int
waitpid(pid_t pid, int *stat_loc, int options)
{
	return (-1);
}
