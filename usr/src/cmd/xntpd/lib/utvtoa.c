/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)utvtoa.c	1.1	96/11/01 SMI"

/*
 * utvtoa - return an asciized representation of an unsigned struct timeval
 */
#include <stdio.h>
#ifndef SYS_WINNT
#include <sys/time.h>
#endif /* SYS_WINNT */

#include "lib_strbuf.h"
#if defined(VMS)
#include "ntp_fp.h"
#endif
#include "ntp_stdlib.h"

char *
utvtoa(tv)
	struct timeval *tv;
{
	register char *buf;

	LIB_GETBUF(buf);
	
	(void) sprintf(buf, "%lu.%06lu", (u_long)tv->tv_sec,
	    (u_long)tv->tv_usec);
	return buf;
}
