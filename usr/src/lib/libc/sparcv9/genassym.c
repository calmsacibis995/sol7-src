/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)genassym.c	1.4	97/12/22 SMI"

#include <sys/synch.h>
#include <sys/synch32.h>

/*
 * This file generates two values used by _lwp_mutex_unlock.s:
 *	a) the byte offset (in lwp_mutex_t) of the word containing the lock byte
 *	b) a mask to extract the waiter field from the word containing it
 */

lwp_mutex_t lm;

main()
{
	(void) printf("#define MUTEX_LOCK_WORD 0x%p\n",
	(void *) ((char *)&lm.mutex_lockw - (char *)&lm));
	(void) printf("#define MUTEX_WAITERS 0x%p\n",
	(void *) ((char *)&lm.mutex_waiters - (char *)&lm));
	printf("#define\tLOCK_MASK\t0xff000000\n");
	return (0);
}
