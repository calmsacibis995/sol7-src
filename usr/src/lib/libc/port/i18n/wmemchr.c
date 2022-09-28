/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wmemchr.c	1.1	97/11/11 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include "libc.h"

wchar_t *
wmemchr(const wchar_t *ws, wchar_t wc, size_t n)
{
	if (n != 0) {
		do {
			if (*ws++ == wc)
				return ((wchar_t *)--ws);
		} while (--n != 0);
	}
	return (NULL);
}
