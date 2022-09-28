/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wcsrtombs.c	1.3	97/12/07 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTIONS: wcsrtombs
 */

#include "file64.h"
#include <stdio.h>
#include <errno.h>
#include <sys/localedef.h>
#include "libc.h"
#include "mse.h"

size_t
wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps)
{
	if (ps == NULL) {
		if ((ps = _get_internal_mbstate(_WCSRTOMBS)) == NULL) {
			errno = ENOMEM;
			return ((size_t)-1);
		}
	}

	return (METHOD(__lc_charmap, wcsrtombs)(__lc_charmap, dst, src,
			len, ps));
}
