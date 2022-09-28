/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)mbsinit.c	1.1	97/11/11 SMI"

/*LINTLIBRARY*/

/*
 *
 * FUNCTIONS: mbsinit
 *
 */

#include <wchar.h>
#include <sys/localedef.h>

int
mbsinit(const mbstate_t *ps)
{
	return (METHOD(__lc_charmap, mbsinit)(__lc_charmap, ps));
}
