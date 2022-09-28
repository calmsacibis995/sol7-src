/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)vsnprintf.c	1.5	97/12/02 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <mtlib.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <synch.h>
#include <thread.h>
#include <sys/types.h>
#include "print.h"

/*VARARGS2*/
int
vsnprintf(char *string, size_t n, const char *format, va_list ap)
{
	ssize_t count;
	FILE siop;

	if (n == 0)
		return (EOF);

	if (n > MAXINT)
		return (EOF);

	siop._cnt = n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD;	/* distinguish dummy file descriptor */
	count = _doprnt(format, ap, &siop);
	*siop._ptr = '\0';	/* plant terminating null character */
	/* overflow check */
	if (count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int) count);
}
