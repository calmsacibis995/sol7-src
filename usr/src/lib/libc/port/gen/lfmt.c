/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)lfmt.c	1.4	96/11/27 SMI"

/*LINTLIBRARY*/

/* lfmt() - format, print and log */

#include "synonyms.h"
#include <sys/types.h>
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include "pfmt_data.h"

int
lfmt(FILE *stream, long flag, const char *format, ...)
{
	int ret;
	va_list args;
	const char *text, *sev;
	va_start(args, ...);

	if ((ret = __pfmt_print(stream, flag, format, &text, &sev, args)) < 0)
		return (ret);

	ret = __lfmt_log(text, sev, args, flag, ret);

	return (ret);
}
