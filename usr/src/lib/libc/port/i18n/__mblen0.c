/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)__mblen_gen.c	1.4	96/12/20 SMI"

/*LINTLIBRARY*/

#include <sys/localedef.h>
#include <stdlib.h>

/*ARGSUSED*/
int
__mblen_gen(_LC_charmap_t * hdl, const char *s, size_t n)
{
	return (mbtowc((wchar_t *)0, s, n));
}
