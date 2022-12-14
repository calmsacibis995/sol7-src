/*
 * Copyright (c) 1991-1994, by Sun Microsystems, Inc.
 */

#pragma ident "@(#)prom_exit.c	1.11	97/10/23 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/bootsvcs.h>

/*
 * The Intel cpu does not have an underlying monitor.
 * So, we do the best we can.....
 */

void
prom_exit_to_mon(void)
{
#ifdef KADB
	printf("Press <CTL>-<ALT>-<DEL> to reboot.\n");
	while (1)
		prom_getchar();
#endif

#ifdef I386BOOT
	prom_printf("Spinning forever\n");
	while (1)
		;
#endif

#if !defined(KADB) && !defined(I386BOOT)
	(void) goany();
	reset();
#endif
}
