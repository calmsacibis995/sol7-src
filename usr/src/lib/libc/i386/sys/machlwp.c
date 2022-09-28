/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)machlwp.c	1.13	97/06/08 SMI"

#pragma weak _lwp_makecontext = __lwp_makecontext

#include "synonyms.h"
#include <memory.h>
#include <fcntl.h>
#include <ucontext.h>
#include <synch.h>
#include <sys/lwp.h>
#include <sys/stack.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/param.h>

extern int __setupgs(void *);
static void *_alloc_gs(void);

/*
 * It is assumed that the ucontext_t structure has already been filled
 * with valid context information.  Here, we update the structure
 * so that when it is passed to _lwp_create() the newly-created
 * lwp will begin execution in the specified function with the
 * specified stack, properly initialized.
 *
 * However, the ucontext_t structure may contain uninitialized data.
 * We must be sure that this does not cause _lwp_create() to malfunction.
 * _lwp_create() only uses the signal mask and the general registers.
 */
void
_lwp_makecontext(
	ucontext_t *ucp,
	void (*func)(),
	void *arg,
	void *private,
	caddr_t stk,
	size_t stksize)
{
	ucontext_t uc;
	uint32_t *stack;

	(void) getcontext(&uc);	/* needed to load segment registers */
	ucp->uc_mcontext.gregs[FS] = uc.uc_mcontext.gregs[FS];
	ucp->uc_mcontext.gregs[ES] = uc.uc_mcontext.gregs[ES];
	ucp->uc_mcontext.gregs[DS] = uc.uc_mcontext.gregs[DS];
	ucp->uc_mcontext.gregs[CS] = uc.uc_mcontext.gregs[CS];
	ucp->uc_mcontext.gregs[SS] = uc.uc_mcontext.gregs[SS];
	if (private)
		ucp->uc_mcontext.gregs[GS] = (greg_t)__setupgs(private);
	else
		ucp->uc_mcontext.gregs[GS] = 0;

	/* top-of-stack must be rounded down to STACK_ALIGN */
	stack = (uint32_t *)(((uintptr_t)stk + stksize) & ~(STACK_ALIGN-1));

	/* set up top stack frame */
	*--stack = 0;
	*--stack = 0;
	*--stack = (uint32_t)arg;
	*--stack = (uint32_t)_lwp_exit;	/* return here if function returns */

	/* fill in registers of interest */
	ucp->uc_flags |= UC_CPU;
	ucp->uc_mcontext.gregs[EIP] = (greg_t)func;
	ucp->uc_mcontext.gregs[UESP] = (greg_t)stack;
	ucp->uc_mcontext.gregs[EBP] = (greg_t)(stack+2);
}

static void **freegsmem = NULL;
static lwp_mutex_t freegslock;

/*
 * Private interface for libthread to acquire/release freegslock.
 * This is needed in libthread's fork1(), where it grabs all internal locks.
 */
void
__freegs_lock()
{
	(void) _lwp_mutex_lock(&freegslock);
}

void
__freegs_unlock()
{
	(void) _lwp_mutex_unlock(&freegslock);
}

int
__setupgs(void *private)
{
	int sel;
	void **priptr;

	(void) _lwp_mutex_lock(&freegslock);
	if ((priptr = freegsmem) !=  NULL)
		freegsmem = *freegsmem;
	else
		priptr = (void **)_alloc_gs();
	(void) _lwp_mutex_unlock(&freegslock);
	if (priptr) {
		sel = __alloc_selector(priptr, 2 * sizeof (void *));
		priptr[0] = private;
		priptr[1] = priptr;
		return (sel);
	}
	return (0);
}

void
__freegs(int sel)
{
	extern void *_getpriptr();
	void *priptr = _getpriptr();
	/*
	 * This is a gross hack that forces the runtime linker to create
	 * bindings for the following global functions. A better solution
	 * is to define static functions that do the same thing as these
	 * global functions. This doesn't work so well either because
	 * static functions can only be called from the file where they
	 * are defined. Scoping is probably the best solution. We could
	 * define new library private functions that can be used by anyone
	 * within this library, and these functions should then be resolved
	 * at link time.
	 * XXX
	 */
	extern void __free_selector();
	int (*lockfunc)() = &_lwp_mutex_lock;
	int (*unlockfunc)() = &_lwp_mutex_unlock;
	void (*freeselfunc)() = &__free_selector;

	if (_lwp_self() != 1) {
		(void) (*lockfunc)(&freegslock);
		*(void **)priptr = freegsmem;
		freegsmem = (void **)priptr;
		(void) (*unlockfunc)(&freegslock);
	}
	(*freeselfunc)(sel);
}

void
_lwp_freecontext(ucontext_t *ucp)
{
	if (ucp->uc_mcontext.gregs[GS] != 0) {
		__freegs(ucp->uc_mcontext.gregs[GS]);
		ucp->uc_mcontext.gregs[GS] = 0;
	}
}

static void *
_alloc_gs()
{
	static int pagesize = 0;
	static int allocsize;
	int fd;
	caddr_t cp, rcp;
	int i, count;

	if (pagesize == 0) {
		pagesize = PAGESIZE;
		allocsize = 2 * sizeof (void *);
	}
	if ((fd = open("/dev/zero", O_RDWR)) == -1)
		return (0);
	cp = mmap(0, pagesize, PROT_READ|PROT_WRITE|PROT_EXEC,
				MAP_PRIVATE, fd, 0);
	if (cp == (caddr_t)(-1)) {
		(void) close(fd);
		return (0);
	}
	count = pagesize / allocsize;
	rcp = cp;
	cp += allocsize;
	count--;
	for (i = 0; i < count; i++) {
		*(void **)cp = freegsmem;
		freegsmem = (void **)cp;
		cp += allocsize;
	}
	(void) close(fd);
	return (rcp);
}
