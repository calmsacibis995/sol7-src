/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wstok.c	1.16	97/12/06 SMI"	/* from JAE2.0 1.0 */

/*	This module is created for NLS on Sep.03.86		*/

/*
 * uses wcspbrk and wcsspn to break string into tokens on
 * sequentially subsequent calls. returns WNULL when no
 * non-separator characters remain.
 * 'subsequent' calls are calls with first argument WNULL.
 */

/*LINTLIBRARY*/

#pragma weak wstok = _wstok

#ifndef WNULL
#define	WNULL	(wchar_t *)0
#endif

#include "mtlib.h"
#include "mse_int.h"
#include <stdlib.h>
#include <wchar.h>
#include <thread.h>
#include "libc.h"

#if 0
static wchar_t **
_tsdget(thread_key_t *key, int size)
{
	wchar_t *loc = 0;

	if (_thr_getspecific(*key, (void **) &loc) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (0);
		}
	}
	if (!loc) {
		if (_thr_setspecific(*key, (void *)(loc = malloc(size))) != 0) {
			if (loc)
				(void) free(loc);
			return (0);
		}
	}
	return ((wchar_t **) loc);
}
#endif

wchar_t *
__wcstok_xpg5(wchar_t *string, const wchar_t *sepset, wchar_t **ptr)
{
	wchar_t *q, *r;

	/* first or subsequent call */
	if ((string == WNULL && (string = *ptr) == 0) ||
	    (((q = string + wcsspn(string, sepset)) != WNULL) && *q == L'\0'))
		return (WNULL);

	/* sepset becomes next string after separator */
	if ((r = wcspbrk(q, sepset)) == WNULL)	/* move past token */
		*ptr = 0;	/* indicate this is last token */
	else {
		*r = L'\0';
		*ptr = r + 1;
	}
	return (q);
}


wchar_t *
wcstok(wchar_t *string, const wchar_t *sepset)
{
	static wchar_t *statlasts;
	static thread_key_t key = 0;
	wchar_t **lasts = (_thr_main() ? &statlasts
			    : (wchar_t **)_tsdalloc(&key, sizeof (wchar_t *)));

	return (__wcstok_xpg5(string, sepset, lasts));
}

wchar_t *
_wstok(wchar_t *string, const wchar_t *sepset)
{
	return (wcstok(string, sepset));
}
