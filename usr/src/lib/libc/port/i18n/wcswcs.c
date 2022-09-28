/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

/*
 * Copyright (c) 1993-1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)wcswcs.c	1.3	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Returns a pointer to the first occurrnce of ws1 in ws2.
 */

#pragma weak wcswcs = _wcswcs

#include <stdlib.h>

wchar_t *
_wcswcs(const wchar_t *ws1, const wchar_t *ws2)
{
	const wchar_t *s1, *s2;
	const wchar_t *tptr;
	wchar_t c;

	s1 = ws1;
	s2 = ws2;

	if (s2 == NULL || *s2 == 0)
		return ((wchar_t *)s1);
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c)
				;
			if (c == 0)
				return ((wchar_t *)tptr - 1);
			s1 = tptr;
			s2 = ws2;
			c = *s2;
		}
	return (NULL);
}
