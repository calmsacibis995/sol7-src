/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
#pragma ident	"@(#)__fgetwc_sb.c 1.5	97/12/02 SMI"

/*LINTLIBRARY*/

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T		*/
/*	  All Rights Reserved  					*/
/*								*/
/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <sys/localedef.h>
#include "libc.h"
#include "stdiom.h"

/*ARGSUSED*/
wint_t
__fgetwc_sb(_LC_charmap_t * hdl, FILE *iop)
{
	int c;

	return (((c = (GETC(iop))) == EOF) ? WEOF : (wint_t) c);
}
