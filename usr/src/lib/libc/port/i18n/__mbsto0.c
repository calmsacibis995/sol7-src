/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbstowcs_dense.c	1.3	96/12/20 SMI"

/*LINTLIBRARY*/

#include <stdlib.h>
#include <sys/localedef.h>
#include <string.h>

size_t
__mbstowcs_dense(_LC_charmap_t * hdl, wchar_t *pwcs, const char *s, size_t n)
{
	int	val;
	size_t	i, count;

	if (pwcs == NULL)
		count = strlen(s);
	else
		count = n;

	for (i = 0; i < count; i++) {
		if ((val = METHOD_NATIVE(hdl, mbtowc)
				(hdl, pwcs, s, MB_CUR_MAX)) == -1)
			return (val);
		if (val == 0)
			break;
		s += val;
		if (pwcs != NULL)
			pwcs++;
	}
	return (i);
}
