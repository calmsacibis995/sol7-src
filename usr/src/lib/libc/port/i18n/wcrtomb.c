/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wcrtomb.c	1.3	97/12/07 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTION: wcrtomb
 */

#include "file64.h"
#include <stdio.h>
#include <errno.h>
#include <sys/localedef.h>
#include "libc.h"
#include "mse.h"

size_t
wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
	if (ps == NULL) {
		if ((ps = _get_internal_mbstate(_WCRTOMB)) == NULL) {
			errno = ENOMEM;
			return ((size_t)-1);
		}
	}

	return (METHOD(__lc_charmap, wcrtomb)(__lc_charmap, s, wc, ps));
}
