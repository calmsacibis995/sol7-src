/*
 * Copyright (c) 1997, 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbsinit_gen.c	1.6	98/02/18 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTION: __mbsinit_gen
 */
#include <sys/localedef.h>
#include <wchar.h>
#include "libc.h"

int
__mbsinit_gen(_LC_charmap_t * hdl, const mbstate_t *ps)
{
	if (ps == NULL)
		return (1);

	if (__mbst_get_nconsumed(ps) == 0)
		return (1);

	return (0);
}
