/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wmemcmp.c	1.1	97/11/11 SMI"
/*LINTLIBRARY*/

#include <sys/types.h>
#include <wchar.h>
#include "libc.h"

int
wmemcmp(const wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	if ((ws1 != ws2) && (n != 0)) {
		do {
			if (*ws1 != *ws2)
				return (*ws1 - *ws2);
			ws1++;
			ws2++;
		} while (--n != 0);
	}
	return (0);
}
