/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)raise.c	1.11	97/08/08 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <thread.h>
#include "mtlib.h"
#include "libc.h"

int
raise(int sig)
{
	if (_thr_main() == -1)
		return (kill(getpid(), sig));
	else
		return (_thr_kill(_thr_self(), sig));
}
