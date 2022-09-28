/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wmemmove.c	1.2	97/12/06 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include <limits.h>
#include <string.h>
#include "libc.h"

wchar_t *
wmemmove(wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	wchar_t	*ows1;
	size_t	max = SIZE_MAX / sizeof (wchar_t);

	if (n <= max) {
		return ((wchar_t *)memmove((void *)ws1,
			(const void *)ws2, n * sizeof (wchar_t)));
	}

	ows1 = ws1;
	if (n != 0) {
		if (ws1 <= ws2) {
			do {
				*ws1++ = *ws2++;
			} while (--n != 0);
		} else {
			ws2 += n;
			ws1 += n;
			do {
				*--ws1 = *--ws2;
			} while (--n != 0);
		}
	}
	return (ows1);
}
