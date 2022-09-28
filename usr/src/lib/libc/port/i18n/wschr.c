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

#pragma ident	"@(#)wschr.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Return the ptr in sp at which the character c appears;
 * Null if not found.
 */

#pragma weak wcschr = _wcschr
#pragma weak wschr = _wschr

#include <stdlib.h>
#include <wchar.h>

wchar_t *
_wcschr(const wchar_t *sp, wchar_t c)
{
	do {
		if (*sp == c)
			return ((wchar_t *)sp); /* found c in sp */
	} while (*sp++);
	return (NULL); /* c not found */
}

wchar_t *
_wschr(const wchar_t *sp, wchar_t c)
{
	return (wcschr(sp, c));
}
