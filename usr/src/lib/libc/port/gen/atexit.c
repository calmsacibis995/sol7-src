/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)atexit.c	1.13	97/06/21 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include <mtlib.h>
#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>

#define	MAXEXITFNS	37

static void	(*exitfns[MAXEXITFNS])(void);
static int	numexitfns = 0;
static int	exithandler_once;
#ifdef _REENTRANT
static mutex_t exitfns_lock = DEFAULTMUTEX;
static mutex_t exithandler_once_lock = DEFAULTMUTEX;
#endif _REENTRANT

int
atexit(void (*func)(void))
{
	int ret = 0;

	(void) _mutex_lock(&exitfns_lock);
	if (numexitfns >= MAXEXITFNS)
		ret = -1;
	else
		exitfns[numexitfns++] = func;
	(void) _mutex_unlock(&exitfns_lock);
	return (ret);
}


void
_exithandle(void)
{
	(void) _mutex_lock(&exithandler_once_lock);
	if (!exithandler_once) {
		exithandler_once = 1;
	} else {
		(void) _mutex_unlock(&exithandler_once_lock);
		return;
	}
	(void) _mutex_unlock(&exithandler_once_lock);
	(void) _mutex_lock(&exitfns_lock);
	while (--numexitfns >= 0)
		(*exitfns[numexitfns])();
	(void) _mutex_unlock(&exitfns_lock);
}
