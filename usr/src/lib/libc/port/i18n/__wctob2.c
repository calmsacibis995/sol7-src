/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wctob_sb.c	1.2	97/11/13 SMI"

/*LINTLIBRARY*/

#include <stdio.h>
#include <wchar.h>
#include <sys/localedef.h>

/*ARGSUSED*/	/* hdl not used here, but needed for interface, don't remove */
int
__wctob_sb(_LC_charmap_t *hdl, wint_t c)
{
	if ((c < 0) || (c > 255))
		return (EOF);

	return (c);
}
