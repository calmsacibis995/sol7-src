/*
 * addrstr.c: "Formulate an address string".
 *
 * SYNOPSIS
 *	const char *addrstr(const void *ad, int nchars, int hex, int sep)
 *	const char *multiAddrstr(const void *, int nchars, int hex, int multi,
 *				int sep)
 *
 * DESCRIPTION
 *	multiAddrstr:
 *		Format a byte sequence into human readable form with bytes
 *		printed	as decimal or hex numbers and separated by a suitable
 *		delimiter. A pointer to a static buffer containing the
 *		character string created is returned. This pointer should not,
 *		therefore, be free'd. Successive calls to multiAddrstr with
 *		"multi" non zero will allocate additional strings but will
 *		not overwrite existing strings. When "multi" is zero, all
 *		previous strings will be overwritten.
 *
 * BUGS
 *	Addrstr() cannot be called twice without the second call
 *	overwriting the previous result. Thus code like:
 *		printf("%s %s", addrstr(...), addrstr(..))
 *	will not work.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)addrstr.c 1.2 96/11/17 SMI"

#include "utils.h"
#include <string.h>
#include <stdio.h>

#define	STATICBUF	1

#if STATICBUF
static char buf[256];
#else
static int size = 0;
static char *buf = 0;
#endif
static int offset = 0;

const char *
addrstr(const void *ad, int nchars, int hex, int sep)
{
	return (multiAddrstr(ad, nchars, hex, 0, sep));
}

const char *
multiAddrstr(const void *ad, int nchars, int hex, int multi, int sep)
{
	register int i;
	int l;
	char *fmt;
	const char *q = (const char *)ad;
	char *b;

	l = 4 * nchars;	/* allow 3 digits + '.' or '\0' for each char */
	if (multi == 0)
		offset = 0;

#if !defined(STATICBUF)
	if ((l + offset) > size) {
		size = l + offset;
		buf = (char *)xrealloc(buf, size);
	}
#endif

	if (hex)
		fmt = "%02x";
	else
		fmt = "%d";
	b = buf + offset;
	(void) sprintf(b, fmt, q[0] & 0xff);
	for (i = 1; i < nchars; i++) {
		l = strlen(b);
		if (sep != 0)
			b[l++] = sep;
		sprintf(b + l, fmt, q[i] & 0xff);
	}
	offset += 1 + strlen(b);
	return (b);
}
