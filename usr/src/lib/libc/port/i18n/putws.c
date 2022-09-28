/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

#pragma ident	"@(#)putws.c	1.14	97/12/07 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Putws transforms process codes in wchar_t array pointed to by
 * "ptr" into a byte string in EUC, and writes the string followed
 * by a new-line character to stdout.
 */

#include "file64.h"
#include <stdio.h>
#include <stdlib.h>
#include <widec.h>
#include <limits.h>
#include <sys/localedef.h>
#include "stdiom.h"

int
putws(const wchar_t *ptr)
{
	wchar_t *ptr0 = (wchar_t *)ptr;
	ptrdiff_t diff;
	rmutex_t	*lk;

	FLOCKFILE(lk, stdout);
	for (; *ptr; ptr++) {		/* putwc till NULL */
		if (putwc(*ptr, stdout) == EOF) {
			FUNLOCKFILE(lk);
			return (EOF);
		}
	}
	(void) putwc('\n', stdout); /* append a new line */
	FUNLOCKFILE(lk);

	if (fflush(stdout))  /* flush line */
		return (EOF);
	diff = ptr - ptr0;
	if (diff <= INT_MAX)
		return ((int)diff);
	else
		return (EOF);
}
