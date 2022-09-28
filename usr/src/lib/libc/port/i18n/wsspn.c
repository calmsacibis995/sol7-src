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

#pragma ident	"@(#)wsspn.c	1.6	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters from charset.
 */

#include <stdlib.h>

size_t
wcsspn(const wchar_t *string, const wchar_t *charset)
{
	const wchar_t *p, *q;

	for (q = string; *q; ++q) {
		for (p = charset; *p && *p != *q; ++p)
			;
		if (*p == 0)
			break;
	}
	return (q - string);
}

size_t
wsspn(const wchar_t *string, const wchar_t *charset)
{
	return (wcsspn(string, charset));
}
