/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)scanf.c	1.16	97/12/07 SMI"	/* SVr4.0 1.13	*/

/*LINTLIBRARY*/
#include "synonyms.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include <synch.h>
#include "libc.h"
#include "stdiom.h"
#include "mse.h"

/*VARARGS1*/
int
scanf(const char *fmt, ...)
{
	rmutex_t	*lk;
	int	ret;
	va_list ap;

	va_start(ap, /* null */);

	FLOCKFILE(lk, stdin);

	_set_orientation_byte(stdin);

	ret = _doscan(stdin, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

/*VARARGS2*/
int
fscanf(FILE *iop, const char *fmt, ...)
{
	rmutex_t	*lk;
	int	ret;
	va_list ap;

	va_start(ap, /* null */);

	FLOCKFILE(lk, iop);

	_set_orientation_byte(iop);

	ret = _doscan(iop, fmt, ap);
	FUNLOCKFILE(lk);
	return (ret);
}

/*VARARGS2*/
int
sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	FILE strbuf;

	va_start(ap, /* null */);
	/*
	 * The dummy FILE * created for sscanf has the _IOWRT
	 * flag set to distinguish it from scanf and fscanf
	 * invocations.
	 */
	strbuf._flag = _IOREAD | _IOWRT;
	strbuf._ptr = strbuf._base = (unsigned char *)str;
	strbuf._cnt = strlen(str);
	strbuf._file = _NFILE;

	/* as this stream is local to this function, no locking is be done */
	return (__doscan_u(&strbuf, fmt, ap));
}
