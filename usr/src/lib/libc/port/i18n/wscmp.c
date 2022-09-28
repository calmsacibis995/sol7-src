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

#pragma ident	"@(#)wscmp.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */

#pragma weak wcscmp = _wcscmp
#pragma weak wscmp = _wscmp

#include <stdlib.h>
#include <wchar.h>

int
_wcscmp(const wchar_t *s1, const wchar_t *s2)
{
	if (s1 == s2)
		return (0);

	while (*s1 == *s2++)
		if (*s1++ == 0)
			return (0);
	return (*s1 - *(s2 - 1));
}

int
_wscmp(const wchar_t *s1, const wchar_t *s2)
{
	return (wcscmp(s1, s2));
}
