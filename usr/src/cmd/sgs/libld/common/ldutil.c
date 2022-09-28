/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)ldutil.c	1.13	98/01/21 SMI"

/*
 * Utility functions
 */
#include	<stdarg.h>
#include	<unistd.h>
#include	<stdio.h>
#include	<signal.h>
#include	<locale.h>
#include	"msg.h"
#include	"_ld.h"

static Ofl_desc *	Ofl;

/*
 * Exit after cleaning up
 */
void
ldexit()
{
	(void) signal(SIGINT, SIG_IGN);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_IGN);
	(void) signal(SIGHUP, SIG_DFL);

	/*
	 * If we have created an output file remove it.
	 */
	if (Ofl->ofl_fd > 0)
		(void) unlink(Ofl->ofl_name);
	lds_atexit(EXIT_FAILURE);
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

/*
 * Trap signals so as to call ldexit(), and initialize allocator symbols.
 */
int
init(Ofl_desc * ofl)
{
	/*
	 * Initialize the output file descriptor address for use in the
	 * signal handler routine.
	 */
	Ofl = ofl;

	if (signal(SIGINT, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	if (signal(SIGHUP, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGHUP, SIG_IGN);
	if (signal(SIGQUIT, (void (*)(int)) ldexit) == SIG_IGN)
		(void) signal(SIGQUIT, SIG_IGN);

	return (1);
}
