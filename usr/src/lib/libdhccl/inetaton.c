/*
 * inetaton.c: "Convert dotted IP notation to IP address".
 *
 * SYNOPSIS
 *    inet_aton(const char *cp, struct in_addr *addr)
 *
 * DESCRIPTION
 *    Check whether "cp" is a valid ascii representation of an Internet
 *    address and convert to a binary address in host byte order.
 *    This replaces inet_addr(), the return value from which cannot
 *    distinguish between failure and an "all ones" broadcast address
 *    (255.255.255.255)
 *
 *    Three formats are supported:
 *        a.b.c.d
 *        a.b.c    (with c treated as 16-bits)
 *        a.b      (with b treated as 24 bits)
 *
 *    The radix of each numeric part of the address may be specified
 *    independantly of the others as in the C-language:
 *    (N represents one or more digits)
 *
 *        0xN or 0XN = hex
 *        0N         = octal
 *        N          = decimal (if first digit is non-zero)
 *
 * RETURNS
 *    1 if the address is valid
 *    0 if not.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)inetaton.c	1.2	96/11/20 SMI"

#include "catype.h"
#include <ctype.h>
#include <netinet/in.h>

int
inet_aton(register const char *cp, struct in_addr *addr)
{
	register uint32_t val, base, n;
	register char c;
	uint32_t parts[4], *pp = parts;

	/*
	 * Collect number up to ``.''.  Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal
	 */
	for (;;) {
		val = 0;
		base = 10;
		if (*cp == '0') {
			if (*++cp == 'x' || *cp == 'X') {
				base = 16;
				cp++;
			} else
				base = 8;
		}
		while ((c = *cp) != '\0') {
			if (isascii(c) && isdigit(c)) {
				val = (val * base) + (c - '0');
				cp++;
			} else if (base == 16 && isascii(c) && isxdigit(c)) {
				val = (val << 4) + (c + 10 - (islower(c) ?
				    'a' : 'A'));
				cp++;
			} else
				break;
		}
		if (*cp == '.') {
			if (pp >= (parts + 3) || val > 0xff)
				return (0);
			*pp++ = val;
			cp++;
		} else
			break;
	}

	/* Check that there are no trailing spaces: */
	if (*cp && (!isascii(*cp) || !isspace(*cp)))
		return (0);

	n = pp - parts + 1;
	switch (n) {
	case 1:
		/* a -- 32 bits */
		break;
	case 2:
		/* a.b -- 8.24 bits */
		if (val > 0xffffff)
			return (0);
		val |= parts[0] << 24;
		break;
	case 3:
		/* a.b.c -- 8.8.16 bits */
		if (val > 0xffff)
			return (0);
		val |= (parts[0] << 24) |  (parts[1] << 16);
		break;
	case 4:
		/* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff)
			return (0);
		val |= (parts[0] << 24) |  (parts[1] << 16) |  (parts[2] << 8);
		break;
	}
	if (addr)
		addr->s_addr = htonl(val);
	return (1);
}
