/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wctob_euc.c	1.3	97/12/07 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <sys/localedef.h>

/*ARGSUSED*/	/* *hdl required for interface, don't remove */
int
__wctob_euc(_LC_charmap_t * hdl, wint_t c)
{
	if ((c & ~0xff) == 0) { /* ASCII or control code. */
		return ((unsigned int) c);
	}
	return (EOF);
}
