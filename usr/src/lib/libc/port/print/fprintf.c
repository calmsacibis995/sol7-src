/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fprintf.c	1.21	97/12/07 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

/* This function should not be defined weak, but there might be */
/* some program or libraries that may be interposing on this */
#pragma weak fprintf = _fprintf

#include "synonyms.h"
#include "shlib.h"
#include <mtlib.h>
#include <thread.h>
#include <synch.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include "print.h"
#include <sys/types.h>
#include "mse.h"

/*VARARGS2*/
int
fprintf(FILE *iop, const char *format, ...)
{
	ssize_t count;
	ssize_t retval;
	rmutex_t *lk;
	va_list ap;

	va_start(ap, /* null */);

	/* Use F*LOCKFILE() macros because fprintf() is not async-safe. */
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
	/* error on overflow */
	if (retval > MAXINT) {
		errno = EOVERFLOW;
		retval = EOF;
	}
	return ((int)retval);
}
