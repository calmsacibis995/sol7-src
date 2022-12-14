/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_lwp_mutex_unlock.s	1.5	98/01/14 SMI"

	.file "_lwp_mutex_unlock.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_mutex_unlock,function)

#include "SYS.h"
#include <sys/synch32.h>
#include "../assym.s"

/*
 * int
 * _lwp_mutex_unlock (mp)
 *	lwp_mutex_t *mp;
 *
 * NOTE: Do not add any calls to procedures or references to global variables
 * or anything like that in this routine, which may cause the run-time linker
 * to be invoked. The reason is that libthread, which uses this routine, calls
 * it with a very small stack during bound thread termination and the stack is
 * not big enough for the linker to execute on.

 * ** NOTE **
 * Also, do not execute a "save/restore" instruction here or call a procedure
 * that might do a save/restore. This is to protect the %o's used by lwps which
 * are terimnating in libthread, since a terminating LWP switches to a global
 * stack and then calls _lwp_mutex_unlock(). Otherwise, the %i's could be
 * damaged by LWP preemption, resulting in corrupted %o's on the return.
 * Note that using any of the %o's as scratch registers below is OK - i.e. none
 * of them have to be preserved for the caller. Just don't do a save/restore
 * - that's all.
 */
        .global __lcmwb
	ENTRY(_lwp_mutex_unlock)
	add	%o0, MUTEX_LOCK_WORD, %g4
	membar	#LoadStore|#StoreStore
	clrn	[%g4]				! clear lock
	clr	%o1
	membar	#StoreLoad
__lcmwb:
	ldub	[%o0+MUTEX_WAITERS], %o1	! read waiters into %o1:
						! could seg-fault
	cmp	%o1, %g0
	be,a	.ret				! check if waiters, if not
	  clr	%o0				! return 0
						! else (note that %o0 is still
						!	&mutex)
	SYSTRAP(lwp_mutex_wakeup)		! call kernel to wakeup waiter
	SYSLWPERR
.ret: 	retl
	nop
	SET_SIZE(_lwp_mutex_unlock)
