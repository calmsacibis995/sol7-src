/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wcsstr.c	1.1	97/11/11 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include "libc.h"

wchar_t *
wcsstr(const wchar_t *ws1, const wchar_t *ws2)
{
	const wchar_t	*os1, *os2;
	const wchar_t	*tptr;
	wchar_t	c;

	os1 = ws1;
	os2 = ws2;

	if (os1 == NULL || *os2 == L'\0')
		return ((wchar_t *)os1);
	c = *os2;

	while (*os1)
		if (*os1++ == c) {
			tptr = os1;
			while (((c = *++os2) == *os1++) &&
				(c != L'\0'))	/* LINTED */
				;
			if (c == L'\0')
				return ((wchar_t *)tptr - 1);
			os1 = tptr;
			os2 = ws2;
			c = *os2;
		}
	return (NULL);
}
