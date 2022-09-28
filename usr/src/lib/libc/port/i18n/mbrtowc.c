/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */
#pragma ident	"@(#)mbrtowc.c	1.4	97/12/07 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTION: mbrtowc
 */
#include "file64.h"
#include <sys/localedef.h>
#include <wchar.h>
#include <errno.h>
#include "libc.h"
#include "mse.h"

size_t
mbrtowc(wchar_t *pwc, const char *s, size_t len, mbstate_t *ps)
{
	if (ps == NULL) {
		if ((ps = _get_internal_mbstate(_MBRTOWC)) == NULL) {
			errno = ENOMEM;
			return ((size_t)-1);
		}
	}

	return (METHOD(__lc_charmap, mbrtowc)(__lc_charmap, pwc, s, len, ps));
}
