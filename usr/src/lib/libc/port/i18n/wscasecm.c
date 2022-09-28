/*
 * Copyright (c) 1991-1996, Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wscasecmp.c	1.3	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * Compare strings ignoring case difference.
 *	returns:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 * All letters are converted to the lowercase and compared.
 */

#pragma weak wscasecmp = _wscasecmp

#include <stdlib.h>
#include <widec.h>
#include "libc.h"

int
_wscasecmp(const wchar_t *s1, const wchar_t *s2)
{
	if (s1 == s2)
		return (0);

	while (_towlower(*s1) == _towlower(*s2++))
		if (*s1++ == 0)
			return (0);
	return (_towlower(*s1) - _towlower(*(s2 - 1)));
}
