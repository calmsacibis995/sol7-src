/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strtows.c	1.2	96/12/20 SMI"

/*LINTLIBRARY*/

#include <limits.h>
#include <widec.h>
#include <errno.h>
#include <stdlib.h>

wchar_t *
strtows(wchar_t *s1, char *s2)
{
	size_t ret;

	ret = mbstowcs(s1, s2, TMP_MAX);
	if (ret == (size_t)-1) {
		errno = EILSEQ;
		return (NULL);
	}
	return (s1);
}

char *
wstostr(char *s1, wchar_t *s2)
{
	size_t ret;

	ret = wcstombs(s1, s2, TMP_MAX);
	if (ret == (size_t)-1) {
		errno = EILSEQ;
		return (NULL);
	}
	return (s1);
}
