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

#pragma ident	"@(#)wslen.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Returns the number of non-NULL characters in s.
 */

#pragma weak wcslen = _wcslen
#pragma weak wslen = _wslen

#include <stdlib.h>
#include <wchar.h>

size_t
_wcslen(const wchar_t *s)
{
	const wchar_t *s0 = s + 1;

	while (*s++)
		;
	return (s - s0);
}

size_t
_wslen(const wchar_t *s)
{
	return (wcslen(s));
}
