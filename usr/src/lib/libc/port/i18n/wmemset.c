/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wmemset.c	1.1	97/11/11 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include "libc.h"

wchar_t *
wmemset(wchar_t *ws, wchar_t wc, size_t n)
{
	wchar_t	*ows1 = ws;

	if (n != 0) {
		do {
			*ws++ = wc;
		} while (--n != 0);
	}
	return (ows1);
}
