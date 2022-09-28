/*
 *      Copyright (c) 1996 by Sun Microsystems, Inc.
 *      All Rights Reserved.
 */

#pragma	ident	"@(#)__mbstowcs_sb.c	1.8	97/01/29 SMI"

/*LINTLIBRARY*/

#include <string.h>
#include <sys/localedef.h>
#include <sys/types.h>
#include "libc.h"

/*ARGSUSED*/	/* *hdl required for interface, don't remove */
size_t
__mbstowcs_sb(_LC_charmap_t *hdl, wchar_t *pwcs, const char *ts, size_t n)
{
	unsigned char *s = (unsigned char *)ts;
	size_t	i;

	if (s == NULL)
		return (0);

	if (pwcs == 0)
		return (strlen(ts));

	for (i = 0; i < n; i++) {
		*pwcs = (wchar_t)*s;
		pwcs++;

		if (*s == '\0')
			break;
		else
			s++;

	}
	return (i);
}
