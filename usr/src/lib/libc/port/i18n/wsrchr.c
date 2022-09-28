/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

#pragma ident	"@(#)wsrchr.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Return the ptr in sp at which the character c last appears;
 * Null if not found.
 */

#pragma weak wcsrchr = _wcsrchr
#pragma weak wsrchr = _wsrchr

#include <stdlib.h>
#include <wchar.h>

wchar_t *
_wcsrchr(const wchar_t *sp, wchar_t c)
{
	const wchar_t *r = NULL;

	do {
		if (*sp == c)
			r = sp; /* found c in sp */
	} while (*sp++);
	return ((wchar_t *)r);
}

wchar_t *
_wsrchr(const wchar_t *sp, wchar_t c)
{
	return (wcsrchr(sp, c));
}
