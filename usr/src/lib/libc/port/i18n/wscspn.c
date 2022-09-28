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

#pragma ident	"@(#)wscspn.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters NOT from charset.
 */

#pragma weak wcscspn = _wcscspn
#pragma weak wscspn = _wscspn

#include <stdlib.h>
#include <wchar.h>

size_t
_wcscspn(const wchar_t *string, const wchar_t *charset)
{
	const wchar_t *p, *q;

	for (q = string; *q != 0; ++q) {
		for (p = charset; *p != 0 && *p != *q; ++p)
			;
		if (*p != 0)
			break;
	}
	return (q - string);
}

size_t
_wscspn(const wchar_t *string, const wchar_t *charset)
{
	return (wcscspn(string, charset));
}
