/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_mutex.s	1.6	97/12/22 SMI"

        .file "_mutex.s"
 
#include <sys/asm_linkage.h>
 
#include <sys/synch32.h>
#include "assym.h"

#define LOCK_SHIFT_VAL (FLAG_SIZE * (FLAG_SIZE - 1))

/*
 * Returns > 0 if there are waiters for this lock.
 * Returns 0 if there are no waiters for this lock.
 * Could seg-fault if the lock memory which contains waiter info is freed.
 * The seg-fault is handled by libthread and the PC is advanced beyond faulting
 * instruction.
 *
 * int
 * _mutex_unlock_asm (mp)
 *      mutex_t *mp;
 */
	.global __wrd
	ENTRY(_mutex_unlock_asm)
	membar	#LoadStore|#StoreStore
#ifdef PSR
	clrx	[%o0+MUTEX_LOCK_WORD]		! clear lock
#else
	clrn	[%o0+MUTEX_LOCK_WORD]		! clear lock
#endif /* PSR */
	membar	#StoreLoad
__wrd:	ldx	[%o0+MUTEX_WAITERS], %o0! read waiters into %o1: could seg-fault
	retl
	srlx	%o0, LOCK_SHIFT_VAL, %o0	! return waiters
	SET_SIZE(_mutex_unlock_asm)

/*
 * _lock_try_adaptive(mutex_t *mp)
 *
 * Stores an owner if it successfully acquires the mutex.
 * Returns non-zero on success.
 */
	ENTRY(_lock_try_adaptive)
	add	%o0, MUTEX_LOCK_WORD, %g5
	mov	%g7, %o0
	casx	[%g5], %g0, %o0			! try lock
	cmp	%o0, %g0
	be,a	1f
	  membar	#LoadLoad
	retl
	clr	%o0
1:
	retl
	or	%o0, 0xff, %o0
	SET_SIZE(_lock_try_adaptive)

/*
 * _lock_clear_adaptive(mutex_t *mp)
 *
 * Clear lock and owner, making sure the store is pushed out to the
 * bus on MP systems.  We could also check the owner here.
 */
	ENTRY(_lock_clear_adaptive)
	membar	#LoadStore|#StoreStore
	retl					! return
#ifdef PSR
	clrx	[%o0+MUTEX_LOCK_WORD]		! clear lock
#else
	clrn	[%o0+MUTEX_LOCK_WORD]		! clear lock
#endif /* PSR */
	SET_SIZE(_lock_clear_adaptive)

/*
 * _lock_owner(mutex_t *mp)
 *
 * Return the thread pointer of the owner of the mutex.
 */
	ENTRY(_lock_owner)
	retl
#ifdef PSR
	ldx	[%o0+MUTEX_LOCK_WORD], %o0
#else
	ldn	[%o0+MUTEX_LOCK_WORD], %o0
#endif /* PSR */
	SET_SIZE(_lock_owner)
