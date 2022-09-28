/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wmemcpy.c	1.1	97/11/11 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include <limits.h>
#include <string.h>
#include "libc.h"

wchar_t *
wmemcpy(wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	wchar_t	*p1, *p2;
	size_t	len;
	size_t	max = SIZE_MAX / sizeof (wchar_t);

	if (n <= max) {
		return ((wchar_t *)memcpy((void *)ws1,
			(const void *)ws2, n * sizeof (wchar_t)));
	}

	p1 = ws1;
	p2 = (wchar_t *)ws2;
	do {
		if (n > max) {
			len = max;
		} else {
			len = n;
		}
		(void) memcpy((void *)p1,
			(const void *)p2, len * sizeof (wchar_t));
		n = n - len;
		p1 += len;
		p2 += len;
	} while (n != 0);
	return (ws1);
}
