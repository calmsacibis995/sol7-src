/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1966,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)printf.c	1.18	97/12/07 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <stdarg.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <values.h>
#include "print.h"
#include <sys/types.h>
#include "mse.h"

/*VARARGS1*/
int
printf(const char *format, ...)
{
	ssize_t count;
	ssize_t retval;
	rmutex_t *lk;
	va_list ap;

	va_start(ap, /* null */);

	/* Use F*LOCKFILE() macros because printf() is not async-safe. */
	FLOCKFILE(lk, stdout);

	_set_orientation_byte(stdout);

	if (!(stdout->_flag & _IOWRT)) {
		/* if no write flag */
		if (stdout->_flag & _IORW) {
			/* if ok, cause read-write */
			stdout->_flag |= _IOWRT;
		} else {
			/* else error */
			FUNLOCKFILE(lk);
			errno = EBADF;
			return (EOF);
		}
	}
	count = _doprnt(format, ap, stdout);
	va_end(ap);
	retval = (FERROR(stdout)? EOF: count);
	FUNLOCKFILE(lk);
	/* check for overflow */
	if (retval > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return (retval);
}
