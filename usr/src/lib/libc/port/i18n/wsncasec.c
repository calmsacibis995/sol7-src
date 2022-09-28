/*
 * Copyright (c) 1991-1996, Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wsncasecmp.c	1.4	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * Compare strings ignoring case difference.
 *	returns:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 * All letters are converted to the lowercase and compared.
 */

#pragma weak wsncasecmp = _wsncasecmp

#include <stdlib.h>
#include <widec.h>
#include "libc.h"

int
_wsncasecmp(const wchar_t *s1, const wchar_t *s2, size_t n)
{
	if (s1 == s2)
		return (0);

	n++;
	while (--n > 0 && _towlower(*s1) == _towlower(*s2++))
		if (*s1++ == 0)
			return (0);
	return ((n == 0) ? 0 : (_towlower(*s1) - _towlower(*(s2 - 1))));
}
