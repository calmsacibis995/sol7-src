/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wctrans.c	1.5	97/12/06 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <sys/localedef.h>
#include "libc.h"

wctrans_t
__wctrans_std(_LC_ctype_t *hdl, const char *name)
{
	int i;

	for (i = 1; i <= hdl->ntrans; i++)
		if (strcmp(name, hdl->transname[i].name) == 0)
			return ((wctrans_t)i);

	return (0);
}

wctrans_t
wctrans(const char *name)
{
	return (METHOD(__lc_ctype, wctrans)(__lc_ctype, name));
}
