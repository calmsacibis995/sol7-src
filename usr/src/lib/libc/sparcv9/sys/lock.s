/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock.s	1.3	97/12/22 SMI"

#include <sys/asm_linkage.h>
#include "../assym.s"

/*
 * lock_try(lp)
 *	- returns non-zero on success.
 */
	ENTRY(_lock_try)
	set	LOCK_MASK, %g4
	casx	[%o0], %g0, %g4			! set lock
	cmp	%g4, %g0			! did we get it?
	be,a	1f
	  membar	#LoadLoad
	retl
	clr	%o0
1:
	retl
	mov	0xff, %o0
	SET_SIZE(_lock_try)

/*
 * lock_clear(lp)
 *	- clear lock.  Waiter not resident in lock word. 
 */
	ENTRY(_lock_clear)
	membar	#LoadStore|#StoreStore
	retl
	clrn	[%o0]				! clear lock
	SET_SIZE(_lock_clear)
