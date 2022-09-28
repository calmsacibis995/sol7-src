/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vfprintf.c	1.17	97/12/07 SMI"	/* SVr4.0 1.7.1.4 */

/*LINTLIBRARY*/
#include "synonyms.h"
#include <thread.h>
#include <mtlib.h>
#include <synch.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <sys/types.h>
#include "print.h"
#include "mse.h"

/*VARARGS2*/
int
vfprintf(FILE *iop, const char *format, va_list ap)
{
	ssize_t count;
	ssize_t retval;
	rmutex_t *lk;

	/* Use F*LOCKFILE() macros because vfprintf() is not async-safe. */
	FLOCKFILE(lk, iop);

	_set_orientation_byte(iop);

	if (!(iop->_flag & _IOWRT)) {
		/* if no write flag */
		if (iop->_flag & _IORW) {
			/* if ok, cause read-write */
			iop->_flag |= _IOWRT;
		} else {
			/* else error */
			FUNLOCKFILE(lk);
			errno = EBADF;
			return (EOF);
		}
	}
	count = _doprnt(format, ap, iop);
	retval = (FERROR(iop)? EOF: count);
	FUNLOCKFILE(lk);
	/* check for overflow */
	if (retval > MAXINT) {
		errno = EOVERFLOW;
		return (EOF);
	} else
		return ((int) retval);
}
