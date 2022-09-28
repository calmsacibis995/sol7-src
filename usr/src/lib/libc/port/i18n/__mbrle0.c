/*
 * Copyright (c) 1997, 1998 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbrlen_gen.c	1.4	98/02/18 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <sys/localedef.h>

/*ARGSUSED*/
size_t
__mbrlen_gen(_LC_charmap_t *hdl, const char *s, size_t n, mbstate_t *ps)
{
	return (METHOD(hdl, mbrtowc)(hdl, (wchar_t *)0, s, n, ps));
}
