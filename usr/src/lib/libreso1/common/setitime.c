/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef LINT
static char rcsid[] = "$Id: setitimer.c,v 8.3 1996/11/18 09:09:02 vixie Exp $";
#endif


#pragma ident   "@(#)setitimer.c 1.1     97/12/03 SMI"

#include "port_before.h"

#include <sys/time.h>

#include "port_after.h"

/*
 * Setitimer emulation routine.
 */
#ifndef NEED_SETITIMER
int __bindcompat_setitimer;
#else

int
__setitimer(int which, const struct itimerval *value,
	    struct itimerval *ovalue)
{
	if (alarm(value->it_value.tv_sec) >= 0)
		return (0);
	else
		return (-1);
}
#endif
