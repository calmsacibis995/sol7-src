/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)close.c	1.5	97/08/29 SMI"

#ifdef __STDC__
#pragma weak close = aio_close
#endif

#include <unistd.h>
#include <sys/types.h>
#include "libaio.h"

extern int __uaio_ok;

int
aio_close(int fd)
{
	if (__uaio_ok)
		aiocancel_all(fd);
	return (_close(fd));
}
