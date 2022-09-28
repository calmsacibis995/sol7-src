/*
 * xgets.c: "Read text lines, allocating sufficient space"
 *
 * SYNOPSIS
 *    int xgets(FILE *f, char **b)
 *
 * DESCRIPTION
 *    xgets is a replacement for fgets(), but allows an arbitrary
 *    amount of text to be read up to the next newline character.
 *    It maintains a static buffer which is re-allocated if
 *    necessary. On return the user supplied argument points
 *    to xgets's internal buffer.
 *
 * BUGS
 *    The user supplied argument points to xgets's static internal
 *    storage. If the contents are to be preserved across calls
 *    to xgets, the buffer must be copied.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)xgets.c 1.3 96/11/21 SMI"

#include <stdio.h>
#include "catype.h"
#include "ca_vbuf.h"
#include "utils.h"

int
xgets(FILE *f, struct Vbuf *b)
{
	register int i = 0;
	int c = '\0';

	while ((c = getc(f)) != EOF && c != '\n') {
		if (i == b->len)
			b->dbuf = (char *)xrealloc(b->dbuf, (b->len += BUFSIZ));
		b->dbuf[i++] = (char)c;
	}

	if (c == EOF && i == 0)
		return (EOF);

	if (i == b->len)
		b->dbuf = (char *)xrealloc(b->dbuf, (b->len += BUFSIZ));
	b->dbuf[i] = '\0';

	return (i);
}
