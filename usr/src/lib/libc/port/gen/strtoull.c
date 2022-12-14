/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)strtoull.c	1.4	96/10/15 SMI"

/*LINTLIBRARY*/

#pragma weak strtoull = _strtoull

#include "synonyms.h"
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <stdlib.h>

#define	DIGIT(x)	\
	(isdigit(x) ? (x) - '0' : islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')

#define	MBASE	('z' - 'a' + 1 + 10)


/*
 * The following macro is a local version of isalnum() which limits
 * alphabetic characters to the ranges a-z and A-Z; locale dependent
 * characters will not return 1. The members of a-z and A-Z are
 * assumed to be in ascending order and contiguous
 */
#define	lisalnum(x)	\
	(isdigit(x) || ((x) >= 'a' && (x) <= 'z') || ((x) >= 'A' && (x) <= 'Z'))

u_longlong_t
strtoull(const char *str, char **nptr, int base)
{
	u_longlong_t val;
	int c;
	int xx;
	u_longlong_t	multmax;
	u_longlong_t	ullong_max;
	int neg = 0;
	const char 	**ptr = (const char **)nptr;

	if (ptr != (const char **)0)
		*ptr = str; /* in case no number is formed */

	ullong_max = ULLONG_MAX;   /* from a local version of limits.h */

	if (base < 0 || base > MBASE || base == 1) {
		errno = EINVAL;
		return (0); /* base is invalid -- should be a fatal error */
	}
	if (!isalnum(c = *str)) {
		while (isspace(c))
			c = *++str;
		switch (c) {
		case '-':
			neg++;
			/* FALLTHROUGH */
		case '+':
			c = *++str;
		}
	}
	if (base == 0)
		if (c != '0')
			base = 10;
		else if (str[1] == 'x' || str[1] == 'X')
			base = 16;
		else
			base = 8;
	/*
	 * for any base > 10, the digits incrementally following
	 *	9 are assumed to be "abc...z" or "ABC...Z"
	 */
	if (!lisalnum(c) || (xx = DIGIT(c)) >= base)
		return (0); /* no number formed */
	if (base == 16 && c == '0' && (str[1] == 'x' || str[1] == 'X') &&
		isxdigit(str[2]))
		c = *(str += 2); /* skip over leading "0x" or "0X" */

	multmax = ullong_max / (u_longlong_t)base;
	val = DIGIT(c);
	for (c = *++str; lisalnum(c) && (xx = DIGIT(c)) < base; ) {
		if (val > multmax)
			goto overflow;
		val *= base;
		if (ullong_max - val < xx)
			goto overflow;
		val += xx;
		c = *++str;
	}
	if (ptr != (const char **)0)
		*ptr = str;
	return (neg ? -val : val);

overflow:
	for (c = *++str; lisalnum(c) && (xx = DIGIT(c)) < base; (c = *++str))
		;
	if (ptr != (const char **)0)
		*ptr = str;
	errno = ERANGE;
	return (ullong_max);
}
