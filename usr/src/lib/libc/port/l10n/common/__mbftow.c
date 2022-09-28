/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__mbftowc_dense_pck.c 1.5	97/09/23  SMI"

#include <ctype.h>
#include <stdlib.h>
#include <sys/localedef.h>

int
__mbftowc_dense_pck(_LC_charmap_t * hdl, char *s, wchar_t *wchar,
	int (*f)(void),	int *peekc)
{
	int	c;

	if ((c = (*f)()) < 0)
		return (0);

	/* LINTED */
	*s++ = (char)c;
	if (c <= 0x7f) {
		*wchar = c;
		return (1);
	/* LINTED */
	} else if ((unsigned char)c >= 0x81 && (unsigned char)c <= 0x9f) {
		*s = (*f)();
		if ((unsigned char)*s >= 0x40 && (unsigned char)*s <= 0xfc &&
		    (unsigned char)*s != 0x7f) {
			*wchar = ((wchar_t)(((unsigned char)*s - 0x40) << 5)
				| (wchar_t)((unsigned char)c - 0x81)) + 0x100;
			return (2);
		}
		c = *s;
	/* LINTED */
	} else if ((unsigned char)c >= 0xe0 && (unsigned char)c <= 0xfc) {
		*s = (*f)();
		if ((unsigned char)*s >= 0x40 && (unsigned char)*s <= 0xfc &&
		    (unsigned char)*s != 0x7f) {
			*wchar = ((wchar_t)(((unsigned char)*s - 0x40) << 5)
				| (wchar_t)((unsigned char)c - 0xe0)) + 0x189f;
			return (2);
		}
		c = *s;
	/* LINTED */
	} else if ((unsigned char)c >= 0xa1 && (unsigned char)c <= 0xdf) {
		*wchar = c;
		return (1);
	}

	*peekc = c;
	return (-1);
}
