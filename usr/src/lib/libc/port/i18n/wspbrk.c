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

#pragma ident	"@(#)wspbrk.c	1.7	96/12/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Return ptr to first occurance of any character from 'brkset'
 * in the wchar_t array 'string'; NULL if none exists.
 */

#pragma weak wcspbrk = _wcspbrk
#pragma weak wspbrk = _wspbrk

#include <stdlib.h>
#include <wchar.h>

wchar_t *
_wcspbrk(const wchar_t *string, const wchar_t *brkset)
{
	const wchar_t *p;

	do {
		for (p = brkset; *p && *p != *string; ++p)
			;
		if (*p)
			return ((wchar_t *)string);
	} while (*string++);
	return (NULL);
}

wchar_t *
_wspbrk(const wchar_t *string, const wchar_t *brkset)
{
	return (wcspbrk(string, brkset));
}
