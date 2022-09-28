/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_path.c	1.1	96/12/10 SMI"

#include <sys/promif.h>
#include <sys/promimpl.h>

char *
prom_path_gettoken(register char *from, register char *to)
{
	while (*from) {
		switch (*from) {
		case '/':
		case '@':
		case ':':
		case ',':
			*to = '\0';
			return (from);
		default:
			*to++ = *from++;
		}
	}
	*to = '\0';
	return (from);
}

/*
 * Strip any options strings from an OBP pathname.
 * Output buffer (to) expected to be as large as input buffer (from).
 */
void
prom_strip_options(register char *from, register char *to)
{
	while (*from != (char)0)  {
		if (*from == ':')  {
			while ((*from != (char)0) && (*from != '/'))
				++from;
		} else
			*to++ = *from++;
	}
	*to = (char)0;
}
