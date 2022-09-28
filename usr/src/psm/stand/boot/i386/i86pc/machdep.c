/*
 * Copyright (c) 1994,1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)machdep.c	1.3	98/01/08 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/salib.h>

extern int bgetproplen(struct bootops *, char *, phandle_t);
extern int bgetprop(struct bootops *, char *, caddr_t, int, phandle_t);
extern struct bootops	*bop;

void
setup_aux(void)
{
	extern char *mmulist;
	static char mmubuf[3 * OBP_MAXDRVNAME];
	int plen;

	if (((plen = bgetproplen(bop, "mmu-modlist", 0)) > 0) &&
	    (plen < (3 * OBP_MAXDRVNAME)))
		(void) bgetprop(bop, "mmu-modlist", mmubuf, 0, 0);
	else
		(void) strcpy(mmubuf, "mmu32"); /* default to mmu32 */
	mmulist = mmubuf;
}
