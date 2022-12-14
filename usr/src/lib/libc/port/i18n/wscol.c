/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * Copyright (c) 1988 by Nihon Sun Microsystems K.K.
 */

#pragma ident	"@(#)wscol.c	1.7	96/12/20 SMI"

/*LINTLIBRARY*/

#include <stdlib.h>
#include <widec.h>
#include <sys/localedef.h>

int
wscol(const wchar_t * ws)
{

	int	col = 0;
	int	ret;

	while (*ws) {
		ret = (METHOD(__lc_charmap, wcwidth)(__lc_charmap, *ws));
	/*
	 * Note that this CSIed version has a bit of different behavior from
	 * the original code in which non-printable char may have various
	 * display width assigned.
	 * If this ever cause complain from user, a specific method may be
	 * required to support Solaris specific wide-character funcitons.
	 */
		if (ret == -1) { /* Nonprintable char */
			col++;
		} else {
			col += ret;
		}
		ws++;
	}
	return (col);
}
