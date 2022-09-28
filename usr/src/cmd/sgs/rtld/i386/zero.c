/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)zero.c	1.9	98/01/08 SMI"

#include	<sys/types.h>

/*ARGSUSED2*/
void
zero(caddr_t addr, size_t l, int hint)
{
	int len = (int)l;

	while (len-- > 0) {
		/* Align and go faster */
		if (((int)addr & ((sizeof (int) - 1))) == 0) {
			/* LINTED */
			int *w = (int *)addr;
			/* LINTED */
			while (len > 0) {
				*w++ = 0;
				len -= sizeof (int);
			}
			return;
		}
		*addr++ = 0;
	}
}
