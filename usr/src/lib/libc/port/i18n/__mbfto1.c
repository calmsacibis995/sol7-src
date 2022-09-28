/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)__mbftowc_sb.c 1.4	97/01/29 SMI"

/*LINTLIBRARY*/

#include <sys/localedef.h>
#include "libc.h"

/*ARGSUSED*/
int
__mbftowc_sb(_LC_charmap_t * hdl, char *ts, wchar_t *wchar,
		int (*f)(void), int *peekc)
{
	unsigned char *s = (unsigned char *)ts;
	int c;

	if ((c = (*f)()) < 0)
		return (0);

	*s = (unsigned char)c;

	*wchar = (wchar_t)*s;
	return (1);
}
