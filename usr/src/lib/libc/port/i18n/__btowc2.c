/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__btowc_sb.c	1.3	97/12/07 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <sys/localedef.h>
#include "libc.h"

/*ARGSUSED*/	/* *hdl required for interface, don't remove */
wint_t
__btowc_sb(_LC_charmap_t *hdl, int c)
{
	unsigned int ch = c;

	if (ch <= 0xff) {
		return ((wint_t) c);
	} else {
		return (WEOF);
	}
}
