/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machlwp.c	1.5	97/08/22 SMI"

/*LINTLIBRARY*/

#pragma weak _lwp_makecontext = __lwp_makecontext

#include "synonyms.h"
#include <sys/stack.h>
#include <sys/ucontext.h>
#include <sys/lwp.h>
#include "libc.h"

#undef SA
/*
 * _lwp_makecontext() should assign the new lwp's SP *within* and at top of
 * the address range [stk, stk + stksize]. The standard SA(X) macro
 * rounds *up* X, which could result in the stack aligned address falling
 * above this range. So define a local SA(X) macro which rounds down X.
 */
#define	SA(X) ((X) & ~(STACK_ALIGN-1))

void
_lwp_makecontext(ucontext_t *ucp, void (*func)(void *), void *arg,
    void *private, caddr_t stk, size_t stksize)
{

	ucp->uc_mcontext.gregs[REG_PC] = (long)func;
	ucp->uc_mcontext.gregs[REG_nPC] = (long)func + sizeof (int);
	ucp->uc_mcontext.gregs[REG_O0] = (long)arg;
	ucp->uc_mcontext.gregs[REG_SP] = SA((long)stk + stksize) - STACK_BIAS;
	ucp->uc_mcontext.gregs[REG_G7] = (long)private;
	ucp->uc_mcontext.gregs[REG_O7] = (long)_lwp_exit - 8;
	/*
	 * clear extra register state information
	 */
	(void) _xregs_clrptr(ucp);
}
