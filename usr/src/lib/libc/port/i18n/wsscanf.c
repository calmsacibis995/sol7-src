/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wsscanf.c	1.12	97/12/02 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <widec.h>
#include <string.h>
#include "libc.h"
#include "stdiom.h"

/* forward declarations */
int _vsscanf(char *, const char *, va_list);

/*
 * 	wsscanf -- this function will read wchar_t characters from
 *		    wchar_t string according to the conversion format.
 *		    Note that the performance degrades if the intermediate
 *		    result of conversion exceeds 1024 bytes due to the
 *		    use of malloc() on each call.
 *		    We should implement wchar_t version of doscan()
 *		    for better performance.
 */
#define	MAXINSTR	1024

int
wsscanf(wchar_t *string, const char *format, ...)
{
	va_list		ap;
	size_t		i;
	char		stackbuf[MAXINSTR];
	char		*tempstring = stackbuf;
	size_t		malloced = 0;
	int		j;

	i = wcstombs(tempstring, string, MAXINSTR);
	if (i == (size_t) -1)
		return (-1);

	if (i == MAXINSTR) { /* The buffer was too small.  Malloc it. */
		tempstring = malloc(malloced = MB_CUR_MAX*wslen(string)+1);
		if (tempstring == 0)
			return (-1);
		i = wcstombs(tempstring, string, malloced); /* Try again. */
		if (i == (size_t) -1) {
			free(tempstring);
			return (-1);
		}
	}

	va_start(ap, format);
	j = _vsscanf(tempstring, format, ap);
	va_end(ap);
	if (malloced) free(tempstring);
	return (j);
}

int
_vsscanf(char *str, const char *fmt, va_list ap)
{
	FILE strbuf;

	strbuf._flag = _IOREAD|_IOWRT;
	strbuf._ptr = strbuf._base = (unsigned char *)str;
	strbuf._cnt = strlen(str);
	strbuf._file = _NFILE;
	return (_doscan(&strbuf, fmt, ap));
}
