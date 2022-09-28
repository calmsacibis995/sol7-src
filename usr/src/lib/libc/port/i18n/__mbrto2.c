/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbrtowc_sb.c	1.4	97/12/07 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include <errno.h>
#include <sys/localedef.h>
#include "mse.h"

/*ARGSUSED*/	/* *hdl required for interface, don't remove */
size_t
__mbrtowc_sb(_LC_charmap_t *hdl, wchar_t *pwc, const char *ts, size_t len,
		mbstate_t *ps)
{
	unsigned char *s = (unsigned char *)ts;

	/*
	 * zero bytes contribute to an incomplete, but
	 * potentially valid character
	 */
	if (len == 0) {
		return ((size_t)-2);
	}

	MBSTATE_RESTART(ps);

	/*
	 * If s is NULL return 0
	 */
	if (s == NULL)
		return (0);

	/*
	 * If pwc is not NULL pwc to s
	 * length is 1 unless NULL which has length 0
	 */
	if (pwc != (wchar_t *)NULL)
		*pwc = (wchar_t)*s;
	if (s[0] != '\0')
		return (1);
	else
		return (0);
}
