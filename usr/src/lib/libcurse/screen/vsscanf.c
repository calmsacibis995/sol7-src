/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident	"@(#)vsscanf.c	1.10	97/12/02 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

#include "file64.h"
#include <sys/types.h>
#include <string.h>
#include <stdarg.h>
#include "curses_inc.h"
#include "stdiom.h"

/*
 *	This routine implements _vsscanf (nonportably) until such time
 *	as one is available in the system (if ever).
 */

/*VARARGS2*/
int
_vsscanf(char *buf, char *fmt, va_list ap)
{
	FILE	junk;

#ifdef SYSV
	junk._flag = _IOREAD;
	/* LINTED */
	junk._file = (unsigned char) -1;
	junk._base = junk._ptr = (unsigned char *) buf;
#else
	junk._flag = _IOREAD|_IOSTRG;
	junk._base = junk._ptr = buf;
#endif
	junk._cnt = strlen(buf);
	return (_doscan(&junk, fmt, ap));
}
