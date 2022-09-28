/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef LINT
static char rcsid[] = "$Id: putenv.c,v 8.3 1996/11/18 09:09:00 vixie Exp $";
#endif


#pragma ident   "@(#)putenv.c 1.1     97/12/03 SMI"

#include "port_before.h"
#include "port_after.h"

/*
 * To give a little credit to Sun, SGI,
 * and many vendors in the SysV world.
 */

#if !defined(NEED_PUTENV)
int __bindcompat_putenv;
#else
int
putenv(char *str) {
	char *tmp;

	for (tmp = str; *tmp && (*tmp != '='); tmp++)
		;

	return (setenv(str, tmp, 1));
}
#endif
