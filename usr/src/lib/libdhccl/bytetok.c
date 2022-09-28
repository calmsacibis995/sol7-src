/*
 * bytetok.c: "Convert an external representation of a byte".
 *
 * SYNOPSIS
 *	int bytetok(const char **src, unsigned char *retbyte)
 *
 * DESCRIPTION
 *    "src" is a pointer to a character pointer which in turn points to a
 *    hexadecimal ASCII representation of a byte.  This byte is read, the
 *    character pointer is updated, and the result is deposited into the
 *    byte pointed to by "retbyte".
 *
 *    The usual '0x' notation is allowed but not required.  The number must be
 *    a two digit hexadecimal number.  If the number is invalid, "src" and
 *    "retbyte" are left untouched and -1 is returned as the function value.
 *    Successful calls return 0.
 *
 * RETURNS
 *    0 if valid, -1 if invalid.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)bytetok.c 1.3 96/11/26 SMI"

#include <ctype.h>
#include <stdio.h>
#include "utils.h"

int
bytetok(const char **src, unsigned char *retbyte)
{
	unsigned int v, w;

	if ((*src)[0] == '0' && (*src)[1] == 'x' || (*src)[1] == 'X')
		(*src) += 2;	/* allow 0x for hex, but don't require it */

	if (!isxdigit((*src)[0]))
		return (-1);
	sscanf(*src, "%1x", &v);
	(*src)++;
	if (isxdigit((*src)[0])) {
		sscanf(*src, "%1x", &w);
		(*src)++;
		v = (16 * v) + w;
	}
	*retbyte = v & 0xFF;
	return (0);
}
