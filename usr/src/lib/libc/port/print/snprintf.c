/*
 * Copyright (c) 1996 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)snprintf.c	1.5	97/12/02 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <stdarg.h>
#include <values.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <sys/types.h>
#include "print.h"


/*VARARGS2*/
int
snprintf(char *string, size_t n, const char *format, ...)
{
	ssize_t count;
	FILE siop;
	va_list ap;

	if (n == 0)
		return (EOF);

	if (n > MAXINT)
		return (EOF);

	siop._cnt = n - 1;
	siop._base = siop._ptr = (unsigned char *)string;
	siop._flag = _IOREAD; /* distinguish dummy file descriptor */

	va_start(ap, /* null */);
	count = _doprnt(format, ap, &siop);
	va_end(ap);
	*siop._ptr = '\0'; /* plant terminating null character */
	/* check for overflow */
	if (count > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return (count);
}
