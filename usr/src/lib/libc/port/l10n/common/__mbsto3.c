/*
 *      Copyright (c) 1996 by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma	ident	"@(#)__mbstowcs_dense_pck.c	1.4	97/09/23 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/localedef.h>

extern int	__mbtowc_dense_pck(_LC_charmap_t *, wchar_t *,
	const char *, size_t);

size_t
__mbstowcs_dense_pck(_LC_charmap_t *hdl, wchar_t *pwcs, const char *s,
				size_t n)
{

	int	val;
	size_t	i, count;

	if (pwcs == 0)
		count = strlen(s);
	else
		count = n;

	for (i = 0; i < count; i++) {
	if ((val = __mbtowc_dense_pck(hdl, pwcs, s, MB_CUR_MAX)) == -1)
		return (val);
	if (val == 0)
		break;
	s += val;
	if (pwcs != NULL)
		pwcs++;
	}
	return (i);
}
